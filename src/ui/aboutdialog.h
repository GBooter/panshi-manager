// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/**
 * @file aboutdialog.h
 * 自绘「关于」对话框（替换 DTK 内置 DAboutDialog 的内容）
 *
 * 背景见 aboutdialog.cpp 顶部：deepin 桌面环境的「关于」菜单调的是
 * DApplication 自动挂的 DAboutDialog，用户看到的那一屏就是它。
 *
 * 为什么继承 DAboutDialog 而不是直接用 DDialog：
 *   `DApplication::setAboutDialog()` 的形参类型是 **DAboutDialog***，
 *   传 DDialog* 编译不过。而 DAboutDialog 只开放 setter（productName/
 *   version/description…），其内部两栏布局**没有任何对外接口**可改 ——
 *   字号、权重、间距、留白全部写死在库里，这正是「看着好丑」的根源。
 *   → 继承它只为拿到正确的类型，再把内部内容整体换成自绘布局。
 *
 * 关键约束：DTK 在 `showEvent` 里可能会重建内容，因此换内容必须
 * **幂等且可重复**（见 cpp 的 applyContent()）。
 */
#include <DAboutDialog>
#include <DLabel>
#include <QString>
#include <QWidget>

namespace PanshiUi {

class PanshiCollapsible;

/**
 * 关于对话框的默认最小宽度。
 *
 * 取 600 而非 520 是有原因的：520 下「平台」那行会折行，而行高会被卡片压缩
 * （Qt 的 QLabel+wordWrap sizeHint 不随宽度重算 + QVBoxLayout 优先压缩子项），
 * 第二行被裁。600 能让所有信息行**一行排完**，从根上绕开这类问题。
 */
constexpr int kAboutMinWidth = 600;

/**
 * 外部链接行：整行可点击，悬停有底色与手型光标。
 *
 * 为什么不用 openExternalLinks：那走的是 QLabel 富文本链接，
 * 颜色/下划线/悬停底不好与项目语义色对齐；自绘更可控。
 */
class LinkRow : public QWidget
{
public:
    LinkRow(const QString &label, const QString &url, QWidget *parent = nullptr);

    /** 主题切换：链接色取 Accent，需重刷 */
    void retheme();

protected:
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *) override;

private:
    QLabel *m_title = nullptr;
    Dtk::Widget::DLabel *m_value = nullptr;
    QString m_url;
    bool m_hover = false;
};

class PanshiAboutDialog : public Dtk::Widget::DAboutDialog
{
    Q_OBJECT

public:
    explicit PanshiAboutDialog(QWidget *parent = nullptr);

    /** 主题切换后重刷依赖语义色的部分（链接色 / 悬停底） */
    void retheme();

protected:
    /** DTK 在 showEvent 里可能重建内容 → 每次显示前重新铺一次（幂等） */
    void showEvent(QShowEvent *event) override;

    /**
     * 自绘不透明圆角底。
     * DDialog 自身 `WA_TranslucentBackground` 且不画任何背景，整窗底色原本只能靠
     * 库内 DBlurEffectWidget（WM 合成层，离屏抓不到、真机 alpha 写死半透明）。
     * 改为在本函数画不透明圆角底：这层是 Qt 控件绘制，grab() 可捕获、真机也生效，
     * 整窗恒不透明（圆角外透出桌面＝圆角观感）。DBlurEffectWidget 已在 showEvent
     * 的隐藏循环里被隐藏，避免它再以半透明像素盖上来。
     */
    void paintEvent(QPaintEvent *event) override;

private:
    /** 构建自绘内容（只建一次，挂在 showEvent 里） */
    QWidget *buildBody();

    QWidget *m_body = nullptr;
    Dtk::Widget::DLabel *m_icon = nullptr;
    PanshiUi::LinkRow *m_linkRow = nullptr;
    PanshiCollapsible *m_refBox = nullptr;
    PanshiCollapsible *m_cmdBox = nullptr;
};

} // namespace PanshiUi
