// SPDX-License-Identifier: GPL-3.0-or-later
//
// DCI 解析诊断 panshi-dciquery（开发期专用）
//
// 目的：侧栏「图标看不清」有两种成因，修法完全不同 ——
//   ① **库里根本没有这个图标** → DIconTheme 返回空 → 回落到单色 symbolic
//      或 QStyle 标准图标（就是用户截图里那两个灰色小箭头）；换名字即可；
//   ② **名字对但图形本身是单色/太弱** → 得换别的图标。
// 分清这两者不能靠猜，本工具给出可核对的两项证据：
//   · 文件是否存在（区分 ①）
//   · **实测饱和度**（区分 ②：绘到侧栏真实尺寸 20px 后逐像素取 HSV 的 S 分量）
//
// ⚠️ 本容器 DConfig 不可用 → DTK 拿不到图标主题名 → `DIconTheme::findQIcon()`
//    对**任何** dci 都返回空。此时若据此判「图标不存在」就是**假阴性**。
//    故这里只借 findDciIconFile 定位**文件**，再用 QIcon(文件路径) 直接加载
//    来测像素 —— 绕开主题配置，看到的就是应用在正常 deepin 上会拿到的图形。
//
// 用法：panshi-dciquery [图标名...]

#include <DGuiApplicationHelper>
#include <DIconTheme>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QTextStream>

#include <algorithm>

DGUI_USE_NAMESPACE

namespace {

/**
 * 图标饱和度：绘成 20px（侧栏真实尺寸）后逐像素取 HSV 的 S 分量，返回 90 分位。
 *
 * 为什么用 90 分位而非均值：彩色图标常有大面积近白高光/背景，均值会被拉低；
 * 90 分位更能反映「主色块有没有颜色」。单色 symbolic 的 S 恒为 0，
 * 这个判据能把两类图标干净分开。
 */
qreal iconSaturation(const QIcon &ic, int px)
{
    if (ic.isNull())
        return -1.0;
    QImage img(px, px, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    ic.paint(&p, QRect(0, 0, px, px), Qt::AlignCenter, QIcon::Normal);
    p.end();

    QList<qreal> sats;
    for (int y = 0; y < px; ++y) {
        for (int x = 0; x < px; ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() < 24)      // 透明像素不算
                continue;
            if (c.value() < 24)      // 近黑也算边缘抗锯齿
                continue;
            sats << c.toHsv().saturationF();
        }
    }
    if (sats.isEmpty())
        return -1.0;
    std::sort(sats.begin(), sats.end());
    return sats.at(int(sats.size() * 0.9));
}

QString verdict(qreal s)
{
    if (s < 0)
        return QStringLiteral("加载失败");
    return s < 0.15 ? QStringLiteral("单色 ❌") : QStringLiteral("彩色 ✅");
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTextStream o(stdout);

    const QStringList searchPaths = DIconTheme::dciThemeSearchPaths();
    o << "== DCI 主题搜索路径 ==\n";
    for (const QString &p : searchPaths)
        o << "  " << p << "  [存在=" << (QFileInfo::exists(p) ? QStringLiteral("是")
                                                              : QStringLiteral("否"))
          << "]\n";

    // 主题名 = 搜索路径下的**子目录名**（搜索路径本身是 <root>/icons 这种父目录）
    QStringList themes;
    for (const QString &p : searchPaths) {
        QDir d(p);
        for (const QString &sub : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            themes << sub;
    }
    themes << QStringLiteral("bloom") << QStringLiteral("deepin")
           << QStringLiteral("default");

    QStringList names;
    for (int i = 1; i < argc; ++i)
        names << QString::fromLatin1(argv[i]);
    if (names.isEmpty()) {
        names << QStringLiteral("drive-harddisk") << QStringLiteral("security-high")
              << QStringLiteral("object-locked") << QStringLiteral("document-save")
              << QStringLiteral("software-update-available") << QStringLiteral("go-home")
              << QStringLiteral("preferences-system") << QStringLiteral("help-about")
              << QStringLiteral("safebox") << QStringLiteral("folder-safebox-lock")
              << QStringLiteral("luckybackup") << QStringLiteral("upgrade-tool")
              << QStringLiteral("offline-update-package") << QStringLiteral("uos-installtool")
              << QStringLiteral("downloader") << QStringLiteral("deepin-diskmanager")
              << QStringLiteral("deepin-repair-tools") << QStringLiteral("deepin-system-monitor")
              << QStringLiteral("deepin-home") << QStringLiteral("com.apps.disks")
              << QStringLiteral("sync-ui") << QStringLiteral("computer")
              << QStringLiteral("application-x-archive");
    }

    o << "\n";
    o << QString("%1 %2 %3 %4\n").arg(QStringLiteral("图标名"), -30)
                                 .arg(QStringLiteral("dci文件"), -10)
                                 .arg(QStringLiteral("饱和度"), -8)
                                 .arg(QStringLiteral("判定"));
    o << QString(74, QChar('-')) << "\n";
    int nColor = 0, nMono = 0, nMissing = 0;
    for (const QString &n : names) {
        QString file;
        for (const QString &th : themes) {
            const QString f = DIconTheme::findDciIconFile(n, th);
            if (!f.isEmpty() && QFileInfo::exists(f)) { file = f; break; }
        }
        const qreal s = iconSaturation(file.isEmpty() ? QIcon() : QIcon(file), 20);
        if (file.isEmpty()) ++nMissing;
        else if (s < 0.15) ++nMono;
        else ++nColor;
        o << QString("%1 %2 %3 %4\n")
              .arg(n, -30)
              .arg(file.isEmpty() ? QStringLiteral("不存在")
                                  : QFileInfo(file).fileName(), -10)
              .arg(s < 0 ? QStringLiteral("—") : QString::number(s, 'f', 3), -8)
              .arg(verdict(s));
    }
    o << QString(74, QChar('-')) << "\n";
    o << QString("彩色 %1 / 单色 %2 / 不存在 %3\n")
          .arg(nColor).arg(nMono).arg(nMissing);
    return 0;
}
