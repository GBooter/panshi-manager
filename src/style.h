// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QColor>
#include <DPalette>
#include <DGuiApplicationHelper>

#include "ui/tokens.h"

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

// ----------------------------------------------------------------------
// 颜色来源（UI 重构方案 §2.2）：
//   本文件不持有任何硬编码色值。所有颜色在运行时取自
//   DGuiApplicationHelper::applicationPalette()（即 DPalette），
//   跟随系统主题与用户主题色；主题切换时 main.cpp 会重新应用本表。
//   QSS 仅承担布局职责（间距/圆角/字重），颜色职责归 DPalette。
// ----------------------------------------------------------------------

/** 从 DPalette 取语义色（DTK 扩展角色：TextTips/TextWarning/TextLively 等） */
inline QColor panshiColor(DPalette::ColorType type)
{
    return DGuiApplicationHelper::instance()->applicationPalette().color(type);
}

/** 从 DPalette 取 QPalette 基础角色色（Window/Base/Highlight 等） */
inline QColor panshiColor(QPalette::ColorRole role)
{
    return DGuiApplicationHelper::instance()->applicationPalette().color(role);
}

/** 把带透明度的 over 叠在 under 上，得到不透明的混合色 */
inline QColor panshiBlend(const QColor &under, const QColor &over)
{
    if (over.alpha() >= 255)
        return over;
    if (over.alpha() <= 0)
        return under;
    const int a = over.alpha();
    const int ia = 255 - a;
    return QColor((over.red() * a + under.red() * ia) / 255,
                  (over.green() * a + under.green() * ia) / 255,
                  (over.blue() * a + under.blue() * ia) / 255, 255);
}

/** 语义强调色的柔和底（≈15% 透明度叠在卡片底色上） */
inline QColor panshiSoftFill(DPalette::ColorType type)
{
    QColor fg = panshiColor(type);
    fg.setAlpha(38);
    return panshiBlend(panshiColor(QPalette::Base), fg);
}

inline QColor panshiSoftFill(QPalette::ColorRole role)
{
    QColor fg = panshiColor(role);
    fg.setAlpha(38);
    return panshiBlend(panshiColor(QPalette::Base), fg);
}

inline QString panshiStyleSheet()
{
    const QString bg          = panshiColor(DPalette::Window).name();
    const QString sidebar     = panshiColor(DPalette::Base).name();
    const QString card        = panshiColor(DPalette::Base).name();
    const QString border      = panshiColor(DPalette::FrameBorder).name();
    const QString textSub     = panshiColor(DPalette::TextTips).name();
    // ⚠️ 侧栏列表底色（2026-10-05 暗色故障）。
    //   QListView 是 Qt 原生控件，其背景由 **QPalette::Base** 决定，而 QApplication
    //   的调色板在本进程里**不会跟随 DTK 主题变化**（实测 setPaletteType(Dark) 后
    //   dpal.Base=#282828 而 qpal.Base 仍是 #ffffff）⇒ 暗色下侧栏整条停在白底，
    //   压在深色内容区旁边极其刺眼（用户截图实况）。
    //   DTK 也不允许我们手动 qApp::setPalette 纠正（明令禁止 + 实测不可逆）。
    //   ⇒ 唯一稳妥解：给这两个列表**显式 QSS 底色**，色值仍取自 applicationPalette()
    //   （panshiStyleSheet 每次主题切换都会重建），保持「颜色职责归 DPalette」。
    //   顺带修好搜索框：QLineEdit 同理走 QPalette::Base/Text。
    const QString inputBg     = panshiColor(QPalette::Base).name();
    const QString inputText   = panshiColor(QPalette::Text).name();
    const QString inputBorder = border;
    // 说明（阶段 3-7b）：原先在此处预取的一批语义色变量
    //   accent / accentSoft / danger / dangerSoft / success / successSoft /
    //   warning / warnBox / warnText / purple
    // 已全部删除 —— 它们唯一的使用者是 #panshiUsageBar 的 chunk 切色规则，
    // 该规则随进度条改用 DProgressBar 一并移除后，这些变量成为死代码
    // （未使用局部变量会触发 -Wunused-variable 诊断）。
    // 这是「QSS 只承担布局职责、颜色职责归 DPalette」的最终收口：
    // 本表此后不再出现任何语义色插值，仅保留 bg/sidebar/card/border/textSub
    // 这五个结构性颜色。

    return QStringLiteral(R"QSS(
/* ================= 主窗口 ================= */
QMainWindow, DMainWindow { background: %BG%; }
#panshiContent { background: %BG%; }

/* ================= 侧边栏 ================= */
#panshiSidebar {
    background: %SIDEBAR%;
    border-right: 1px solid %BORDER%;
}
/* 导航项的 hover/选中/焦点态由 PanshiSidebar 的 delegate 用
   DPalette（ItemBackground/Highlight）绘制，不走 QSS（方案 §3.1） */

/* ⚠️ 两个列表必须显式给底色（2026-10-05 暗色故障，勿删）：
   QListView 默认用 QPalette::Base 填 viewport，而这个调色板在本进程里不跟随
   DTK 主题切换 —— 暗色下侧栏会整条停在 #ffffff 白底。色值由本表按当前
   applicationPalette() 生成，主题切换时随 QSS 一起重建。 */
#panshiNavList, #panshiSearchResultList {
    background: %SIDEBAR%;
    border: none;
}
/* 搜索输入框同理：QLineEdit 的底/字色都走 QApplication 调色板。 */
#panshiSearchEdit {
    background: %INPUTBG%;
    color: %INPUTTEXT%;
    border: 1px solid %INPUTBORDER%;
    border-radius: %RADIUS_ELEMENT%;
    padding: 4px 8px;
}
/* 搜索框占位符：DTK 默认取 PlaceholderText 角色（同样来自过期的调色板），
   显式给 Muted 色，否则暗色下是浅底上的浅字 / 亮色下的深底深字。 */
#panshiSearchEdit::placeholder {
    color: %TEXTSUB%;
}

/* ================= 页面标题 ================= */
#panshiPageTitle { font-weight: 600; }
#panshiPageSubtitle { }

/* ================= 卡片 ================= */
#panshiCard {
    background: %CARD%;
    border: 1px solid %BORDER%;
    border-radius: 10px;
}
#panshiCardTitle { font-weight: 600; }
#panshiCardDesc, #panshiRowDesc { }
#panshiRowLabel { }

/* ================= 统计卡 ================= */
/* 已迁移（阶段 3-7）：磁盘空间页区间汇总的 4 个数值卡改用 PanshiStateCard，
   概览大数值改走 DPalette::TextTitle + T2，不再用 panshiStatValue 的
   stat 属性 + QSS 属性选择器染色。原 #panshiStatValue[stat=…] 与
   #panshiStatLabel 规则已随该页重写移除（颜色职责回归组件层）。 */

/* ================= 磐石功能开关行（阶段 4 已迁移，无规则） ================= */
/* PanshiSwitchRow 的副标题语义色走 PanshiUi::applySemantic()（DPalette），
   "详情"按钮为 flat + Accent 前景。原 #panshiSwitchStatus
   （含 [stat=green|orange|red] 属性选择器）与 #panshiLinkBtn / :hover
   已移除——颜色职责全部回到组件层。 */

/* 标签徽章 / 提示条 / 警告框：已全部迁移到 PanshiBadge（自绘柔和底）
   与 PanshiInlineBanner（图标 + 柔和底 + 中性正文），此处不再保留 QSS 规则
   （阶段 3-6；最后一处引用 snapshotspage 随本阶段重写一并移除） */

/* ================= 加载遮罩 ================= */
#panshiMask { background: rgba(0, 0, 0, 0.05); }
#panshiMaskText { }

/* ================= 设置分组 ================= */
#panshiGroupTitle { font-weight: 600; color: %TEXTSUB%; padding-top: 4px; }
#panshiSep { background: %BORDER%; border: none; max-height: 1px; }

/* ================= 概览进度条（使用率语义色） ================= */
/* 阶段 3-7：磁盘空间页概览的「已用比例」条放弃 level 属性 + QSS 切 chunk 色，
   语义改由左侧大数值的文字色承载（DPalette 角色，见 updateOverview）。
   阶段 3-7b：该条已从 QProgressBar 换成 DProgressBar（DTK 原生外观），
   objectName「panshiUsageBar」一并移除 —— 下方原 #panshiUsageBar 的两条
   规则（基础 + ::chunk）随之删除，避免留死规则。
   颜色职责就此完全回归 DPalette / DTK 样式。 */

/* ================= 表格 ================= */
/* QTableWidget 的颜色职责已交还 DPalette（Window/Base/ItemBackground/
   Highlight/FrameBorder 由 DTK 样式统一处理，QSS 不再覆盖）。阶段 3-6
   移除整块表格规则：此前硬编码的 alternate-background-color 与
   selection-background-color 会盖掉 DTK 的深浅主题取值。
   阶段 3-7 已清 #panshiStatValue / #panshiStatLabel / #panshiUsageBar[level]
   （磁盘空间页重写后无引用者）。 */
)QSS")
        .replace(QStringLiteral("%BG%"), bg)
        .replace(QStringLiteral("%SIDEBAR%"), sidebar)
        .replace(QStringLiteral("%CARD%"), card)
        .replace(QStringLiteral("%BORDER%"), border)
        .replace(QStringLiteral("%TEXTSUB%"), textSub)
        .replace(QStringLiteral("%INPUTBG%"), inputBg)
        .replace(QStringLiteral("%INPUTTEXT%"), inputText)
        .replace(QStringLiteral("%INPUTBORDER%"), inputBorder)
        // ⚠️ 须写全限定名：Radius 定义在 namespace PanshiUi 内，而本文件是
        // 全局命名空间的头（main.cpp 直接 include 它）。
        .replace(QStringLiteral("%RADIUS_ELEMENT%"), QString::number(PanshiUi::Radius::Element));
}
