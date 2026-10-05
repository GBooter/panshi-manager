// SPDX-License-Identifier: GPL-3.0-or-later
//
// 关于对话框渲染探针（开发期专用，不安装）
//
// 目的：「关于」窗口是**继承 DAboutDialog 后清空内部内容再自绘**的 —
//      这种「接管别人家布局」的做法最容易被反噬：清不干净会留下空壳、
//      showEvent 被基类覆盖、DDialog 标题栏被误删。
//      这些问题肉眼看真机要等用户点开菜单才知道，太迟。
//      本探针离屏实例化真实对话框并 grab() 出图，同时断言结构，
//      亮/暗两套都出 —— 与 panshi-colorprobe 同一套路。
//
// 用法：panshi-aboutshot [输出前缀]
//   输出 <前缀>_light.png / <前缀>_dark.png，stdout 输出结构断言结果。

#include "ui/aboutdialog.h"

#include <DGuiApplicationHelper>

#include <QApplication>
#include <QPainter>
#include <QLabel>
#include <QPixmap>
#include <QRegion>
#include <QTextStream>

#include <cstdlib>

DWIDGET_USE_NAMESPACE
using namespace PanshiUi;

namespace {

int g_fail = 0;

void check(bool ok, const QString &what)
{
    QTextStream o(stdout);
    o << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    o.flush();
    if (!ok)
        ++g_fail;
}

/** 递归收集可见文本，用于确认关键信息真的渲染出来了 */
QStringList visibleTexts(QWidget *w)
{
    QStringList out;
    for (QWidget *c : w->findChildren<QWidget *>()) {
        if (auto *l = qobject_cast<QLabel *>(c)) {
            // 跳过含替换字符（U+FFFD）的文本：容器 locale 缺失会让部分中文字
            // 串渲染/解码成乱码，那些不是「文案缺失」，混进断言里会造成假失败。
            if (!l->text().trimmed().isEmpty()
                && !l->text().contains(QChar(QChar::ReplacementCharacter)))
                out << l->text().trimmed();
        }
    }
    return out;
}

void shoot(PanshiAboutDialog *d, bool dark, const QString &path)
{
    // 先切主题再 show：布局与调色板都定下来后 grab 才是最终观感。
    // 主题切换走**进程内** setPaletteType —— 本机系统级暗色不可达
    // （DTK6 走 treeland Wayland personalization，不读 gsettings）。
    DGuiApplicationHelper::instance()->setPaletteType(
        dark ? DGuiApplicationHelper::DarkType : DGuiApplicationHelper::LightType);
    // ⚠️ 不要在这里写死 resize(560, sizeHint().height())：
    // 对话框高度由它自己的 showEvent 按内容算好了，探针再 resize 会把它压回
    // 基类的旧 sizeHint（约 290px）→ 抓出来的图是「排版被压扁」的假象，
    // 反而掩盖真实问题。探针只负责切主题 + 抓图。
    d->show();
    qApp->processEvents();
    d->grab().save(path);
    QTextStream o(stdout);
    o << "wrote " << path << "\n";
    o.flush();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationName(QStringLiteral("panshi-manager"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    const QString prefix = argc > 1 ? QString::fromLatin1(argv[1])
                                    : QStringLiteral("/tmp/aboutshot");

    DGuiApplicationHelper::instance()->setPaletteType(
        DGuiApplicationHelper::LightType);
    auto *d = new PanshiAboutDialog;
    d->show();
    qApp->processEvents();

    QTextStream o(stdout);
    o << "== 关于对话框结构断言 ==\n";

    // findChild<T> 要求 T 带 Q_OBJECT，QWidget 没有 → 走指针版
    check(d->findChild<QWidget *>(QStringLiteral("panshiAboutBody")) != nullptr,
          QStringLiteral("自绘内容容器已挂载（panshiAboutBody）"));

    const QStringList texts = visibleTexts(d);
    const auto has = [&texts](const QString &needle) {
        for (const QString &t : texts)
            if (t.contains(needle)) return true;
        return false;
    };

    check(has(QStringLiteral("Feelup")), QStringLiteral("作者信息「Feelup」已呈现"));
    check(has(QStringLiteral("磐石设置")), QStringLiteral("应用名已呈现"));
    check(has(QStringLiteral("1.0.0")), QStringLiteral("版本号已呈现"));
    check(has(QStringLiteral("主页")), QStringLiteral("主页行已呈现"));
    check(has(QStringLiteral("GPL")), QStringLiteral("协议行已呈现"));
    // LinkRow 刻意不带 Q_OBJECT（项目约定：组件层除必须动画的 PanhiRow 外
    // 一律不带，避免 moc 依赖），Qt6.8 的 findChild 模板版对无 Q_OBJECT 的类型
    // 会 static_assert 失败。→ 认它自己设的 accessibleName（"主页：<url>"），
    // 这比认类名更贴近「用户实际能读到什么」。
    bool hasLinkRow = false;
    for (QWidget *w : d->findChildren<QWidget *>()) {
        if (QString::fromLatin1(w->metaObject()->className()).contains(
                QStringLiteral("LinkRow"))
            || w->accessibleName().startsWith(QStringLiteral("主页："))) {
            hasLinkRow = true;
            break;
        }
    }
    check(hasLinkRow, QStringLiteral("主页是可点击的 LinkRow（不是死文本）"));

    // 布局健康度：宽度够、不溢出
    // ---- 标题栏必须保留（2026-10-04 用户反馈「右上角按钮都没了」）----
    // DAboutDialog 有两个直接子控件：内容区 + DTitlebar。隐藏时若不跳过
    // DTitlebar，关闭按钮（DTitlebarDWindowCloseButton）会一起消失。
    bool hasTitlebar = false;
    for (QWidget *w : d->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (QString::fromLatin1(w->metaObject()->className())
                .contains(QStringLiteral("DTitlebar"))) {
            hasTitlebar = true;
            break;
        }
    }
    check(hasTitlebar, QStringLiteral("DTitlebar（关闭按钮所在）仍存在"));
    if (auto *tb = d->findChild<QWidget *>(QStringLiteral("DTitlebarDWindowCloseButton"))) {
        // ⚠️ 不断言 tb->isVisible()：离屏（offscreen）无窗口管理器做 polish，
        // **原版 DAboutDialog 在离屏下这个按钮也是 visible=0**（已实测），
        // 断言它可见会得到与本缺陷无关的假失败。
        // 这里改为断言「祖先链未被本代码隐藏」—— 这才是缺陷 A2 的真实成因。
        bool ancestorsVisible = true;
        for (QWidget *p2 = tb->parentWidget(); p2; p2 = p2->parentWidget()) {
            if (p2->isHidden()) {          // isHidden() = 自身被显式隐藏
                ancestorsVisible = false;
                break;
            }
        }
        check(ancestorsVisible,
              QStringLiteral("关闭按钮的祖先链未被隐藏（离屏下按钮本身不显示属正常）"));
    } else {
        check(false, QStringLiteral("找不到关闭按钮 DTitlebarDWindowCloseButton"));
    }

    // ---- 内容区必须不透明（2026-10-04 用户反馈「透明度太高」）----
    // DDialog 自身背景半透明（圆角+阴影设计），内容区需自补不透明底色，
    // 否则桌面壁纸会整片透进来。
    if (auto *body = d->findChild<QWidget *>(QStringLiteral("panshiAboutBody"))) {
        check(body->autoFillBackground(),
              QStringLiteral("自绘内容区设了不透明底色（autoFillBackground）"));
        QImage probe(40, 40, QImage::Format_ARGB32);
        probe.fill(Qt::transparent);
        QPainter pp(&probe);
        body->render(&pp, QPoint(0, 0), QRegion(), QWidget::DrawChildren);
        pp.end();
        // 取中心像素：alpha 应为 255（完全不透明）
        const QColor c = probe.pixelColor(20, 20);
        check(c.alpha() == 255,
              QStringLiteral("内容区中心像素不透明（alpha=%1）").arg(c.alpha()));
    } else {
        check(false, QStringLiteral("找不到 panshiAboutBody"));
    }

    check(d->width() >= 520, QStringLiteral("宽度不小于设定的下限 520（实际 %1）").arg(d->width()));
    check(d->height() > 300, QStringLiteral("高度合理（实际 %1）").arg(d->height()));

    shoot(d, false, prefix + QStringLiteral("_light.png"));

    DGuiApplicationHelper::instance()->setPaletteType(
        DGuiApplicationHelper::DarkType);
    qApp->processEvents();
    shoot(d, true, prefix + QStringLiteral("_dark.png"));

    o << "\n";
    if (g_fail == 0) {
        o << "PASS: 关于对话框结构与关键信息全部到位\n";
        o.flush();
        return 0;
    }
    o << "FAIL: " << g_fail << " 条断言未通过\n";
    o.flush();
    return 1;
}
