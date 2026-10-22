// SPDX-License-Identifier: GPL-3.0-or-later
// 全项目 wordWrap 标签「高度不足被裁切」检测（开发期验证用，验证后删除）
//
// 判据（2026-10-06 定）：QLabel 设了 wordWrap 后依赖 heightForWidth 协商高度。
// 若被塞进带 alignment 的 addWidget（或父容器高度受限时），布局按 sizeHint()
// （= 自然宽、不折行 ⇒ 单行高）分配空间，不再做 heightForWidth 协商
// ⇒ 控件高度 < 文字实际需要高度 ⇒ 字画到边界外被拦腰截断（用户截图实况）。
// 所以判据是 height() < heightForWidth(width())，**不是**几何相交 ——
// 后者在 wordWrap 场景下是假阴性（实测双向都 PASS，差点误判成"没修好"）。
//
// 遍历全部页面 × 三个宽度（窄窗口最容易暴露问题）。
#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QWidget>
#include <cstdio>

#include "mainwindow.h"

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication a(argc, argv);

    const int widths[] = {880, 700, 560};
    const PageId pages[] = {
        PageId::Overview,      PageId::DiskUsage,
        PageId::BackupVersion, PageId::Protection,
        PageId::Policy,        PageId::Update,
        PageId::About,
    };
    const char *names[] = {"总览", "磁盘空间", "备份与版本",
                           "系统保护", "策略与设置", "系统更新", "关于"};

    MainWindow w;
    int total = 0;
    for (int width : widths) {
        w.resize(width, 720);
        w.show();
        for (int i = 0; i < 6; ++i)
            QCoreApplication::processEvents();

        for (int p = 0; p < 7; ++p) {
            w.gotoPage(pages[p]);
            for (int i = 0; i < 6; ++i)
                QCoreApplication::processEvents();

            for (QWidget *c : w.findChildren<QLabel *>()) {
                auto *lb = qobject_cast<QLabel *>(c);
                // 过滤：空文本 / 不可见（折叠区默认收起，几何是未布局默认值）
                //      / 零尺寸 / 非 wordWrap（不折行的高度不会失配）
                if (!lb || lb->text().isEmpty() || !lb->isVisible() || !lb->wordWrap())
                    continue;
                if (lb->width() <= 0 || lb->height() <= 0)
                    continue;
                const int need = lb->heightForWidth(lb->width());
                if (need > lb->height()) {
                    fprintf(stderr, "  CLIPPED [%s w=%d] [%.30s]  有 %dpx 需要 %dpx\n",
                            names[p], width, qPrintable(lb->text().simplified()),
                            lb->height(), need);
                    ++total;
                }
            }
        }
    }
    fprintf(stderr, total ? "FAIL: 共 %d 处裁切\n" : "PASS: 无裁切\n", total);
    return total ? 1 : 0;
}