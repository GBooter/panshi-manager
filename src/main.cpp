// SPDX-FileCopyrightText: 2026 panshi-manager developers
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * 磐石设置 (Panshi Manager) - DTK 版本
 * Deepin 25 磐石系统管理工具
 */
#include "mainwindow.h"
#include "disk/monitormain.h"
#include "style.h"
#include "pages/common.h"
#include "ui/aboutdialog.h"

#include <DApplication>
#include <DMainWindow>
#include <DWidgetUtil>
#include <DIconTheme>
#include <DLog>
#include <DGuiApplicationHelper>

#include <QIcon>
#include <QDate>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QGuiApplication>   // setHighDpiScaleFactorRoundingPolicy
#include <QList>      // 主题切换时的深度遍历栈
#include <QWidget>
#include <clocale>

#include <QDir>
#include <QSettings>
#include <QTranslator>
#include <QLocale>
DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE
DCORE_USE_NAMESPACE

namespace {

// 磁盘定位转发服务：通知"查看详情"点击时，若应用已在运行，
// 新实例通过 D-Bus 调用本对象将日期转交运行中的主窗口。
class DiskNavObject : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

public slots:
    void ShowDisk(const QString &date)
    {
        const QDate d = QDate::fromString(date, Qt::ISODate);
        MainWindow *w = MainWindow::instance();
        if (!w || !d.isValid())
            return;
        w->show();
        w->raise();
        w->activateWindow();
        w->showDiskDate(d);
    }
};

} // namespace

int main(int argc, char *argv[])
{
    // ⚠️ 必须在**创建任何 QGuiApplication 之前**调用，否则 Qt 打印
    //    "setHighDpiScaleFactorRoundingPolicy must be called before creating
    //     the QGuiApplication instance"（用户启动日志第一条，2026-10-05）。
    //    取 PassThrough：让 1.25x/1.5x 之类的小数缩放**原样透传**给 Qt，
    //    避免被取整成整数倍导致 HiDPI 下布局被重排（深浅色下这类重排尤其明显）。
    //    Deepin 桌面默认给 1.25 缩放，正是最需要透传的档位。
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // 后台监控模式（无 GUI）：由 systemd user timer 拉起
    QDate focusDiskDate;
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--monitor") == 0)
            return Panshi::runMonitor(argc, argv);
        if (qstrcmp(argv[i], "--show-disk") == 0 && i + 1 < argc)
            focusDiskDate = QDate::fromString(QString::fromLocal8Bit(argv[i + 1]), Qt::ISODate);
    }

    // ⚠️ 用户语言偏好（标题栏菜单「语言」）—— 走**显式 QTranslator**，不靠环境变量。
    //
    // 为什么不用环境变量（2026-10-06 三轮实证，全部推翻了直觉）：
    //   ① `QLocale::setDefault(en_US)`                       → 无效
    //   ② `LANGUAGE=en_US`（进程**启动前**就设好）              → ✅ 有效
    //   ③ 同上但**在 main 内部 qputenv**（启动后才设）           → ❌ 无效
    //      诊断证据：env 里 LANGUAGE=en_US、QLocale().name()=en_US 全都对，
    //      但 `QLocale::system()` 仍是 **zh_CN**，译文依旧中文。
    //   根因：DTK 的 loadTranslator() 走 `QLocale::system()`（进程启动时由 C 库
    //   缓存的 locale），**运行期改环境变量已经太晚**，改不动它。
    //   ⇒ 语言偏好必须在**进程启动之前**就确定，因此「改完设置再重启自身」
    //   这条路必须把语言**通过环境变量传给子进程**（见 MainWindow::switchLanguage）。
    //
    // 本函数只负责「按偏好把该进程的 locale 定下来」；真正的译文装载在
    // loadTranslator() 之后**显式 install QTranslator** 兜底（不依赖 DTK 解析）。
    QString g_language;   // 供下方显式装载 .qm 使用
    {
        QSettings langSet(QStringLiteral("deepin"), QStringLiteral("panshi-manager"));
        const QString lang = langSet.value(QStringLiteral("language"),
                                           QStringLiteral("system")).toString();
        // LC_ALL 优先级最高，先清掉，避免残留把下面的选择覆盖掉
        qunsetenv("LC_ALL");
        if (lang == QStringLiteral("en_US") || lang == QStringLiteral("zh_CN")) {
            g_language = lang;
            // ① 显式 setlocale：把 C 库当前 locale 拧到目标语言，
            //    这样 QLocale::system() 才会返回目标语言（DTK 依赖它）。
            setlocale(LC_ALL, lang == QStringLiteral("en_US") ? "en_US.UTF-8"
                                                              : "zh_CN.UTF-8");
            // ② 同时用环境变量表达意图：让**重启后的子进程**在启动前就带上语言，
            //    从而绕过上面「运行期改环境变量太晚」的限制。
            qputenv("LANGUAGE", lang.toUtf8());
            qputenv("LANG", (lang == QStringLiteral("en_US") ? QStringLiteral("en_US.UTF-8")
                                                             : QStringLiteral("zh_CN.UTF-8"))
                               .toUtf8());
            if (lang == QStringLiteral("en_US"))
                QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
            else
                QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::SimplifiedChineseScript,
                                           QLocale::China));
        } else {
            qunsetenv("LANGUAGE");   // "system"：完整交回系统环境
        }
    }

    DApplication app(argc, argv);
    app.setOrganizationName("deepin");
    app.setApplicationName("panshi-manager");
    // 版本号唯一来源：CMakeLists.txt 的 project(... VERSION x.y.z) 经
    // add_compile_definitions 下发为 PANSHI_VERSION。此处不得写字面量，
    // 否则界面版本会与 deb 包版本脱钩（1.0.0~1.0.5 的历史缺陷）。
    app.setApplicationVersion(QStringLiteral(PANSHI_VERSION));

    // ⚠ 时序（DTK 官方约束，见 docs/gotchas.md §7.2 / widgets/application.md §12）：
    //   loadTranslator() 必须在**创建任何可翻译对象之前**调用，否则这些文案
    //   会以源串（中文）落到 QTranslator 装载之前，取不到译文。
    //   以下 setProductName / setApplicationDisplayName / setApplicationDescription
    //   都属"可翻译对象"，此前它们排在 loadTranslator() 之后 → 英文 locale 下
    //   产品名/描述仍是中文。现全部下移到 loadTranslator() 之后。
    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    app.loadTranslator();

    // ③ 显式兜底装载：DTK 的自动解析在「运行期改 locale」场景下不可靠，
    //    这里自己按偏好把对应 .qm 装上，确保语言选择**一定**生效。
    //    放在 loadTranslator() 之后：Qt 按「后装载的优先」查找，
    //    我们的翻译器因此覆盖 DTK 装的那一份。
    if (!g_language.isEmpty()) {
        static QTranslator g_tr;
        const QString qmName = QStringLiteral("panshi-manager_%1.qm").arg(g_language);
        // 搜索顺序：用户级部署目录 → 系统级 → 可执行文件同级（开发期直跑 build/）
        const QStringList dirs = {
            QDir::homePath() + QStringLiteral("/.local/share/panshi-manager/translations"),
            QStringLiteral("/usr/share/panshi-manager/translations"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
            QCoreApplication::applicationDirPath(),
        };
        for (const QString &d : dirs) {
            if (g_tr.load(qmName, d)) {
                QCoreApplication::installTranslator(&g_tr);
                break;
            }
        }
    }

    app.setApplicationDisplayName(QObject::tr("磐石设置"));
    app.setProductName(QObject::tr("磐石设置"));
    // 新 Logo（2026-10-09）：内嵌资源优先（系统目录可能残留旧版图标）。
    {
        QIcon appIcon(QStringLiteral(":/icons/panshi.svg"));
        if (appIcon.isNull())
            appIcon = DIconTheme::findQIcon("panshi");
        app.setProductIcon(appIcon);
    }
    app.setApplicationDescription(QObject::tr("Deepin 25 磐石系统管理工具"));
    app.setApplicationLicense("GPL-3.0-or-later");

    // ⚠️ 接管「关于」对话框（2026-10-04）。
    // DApplication 会**自动挂一个 DAboutDialog**，而 deepin 桌面环境菜单里的
    // 「关于」调的就是它 —— 也就是用户看到的那一屏。DTK 那个对话框的
    // 子部件没有任何对外样式接口，布局全写死在库里，无法调整。
    // → 自绘一个替换掉入口；作者信息也一并在这里补上。
    // 注册需在 DApplication 构造之后、窗口显示之前。
    app.setAboutDialog(new PanshiUi::PanshiAboutDialog);

    // 磁盘定位转发：若已有实例在运行，把日期转交过去后退出
    QDBusConnection bus = QDBusConnection::sessionBus();
    // 注意：D-Bus 总线名允许连字符，但接口名不允许；两者分开命名
    const QString navService = QStringLiteral("org.deepin.panshi-manager.disknav");
    const QString navInterface = QStringLiteral("org.deepin.panshi.disknav");
    const bool navRegistered = bus.registerService(navService);
    if (!navRegistered && focusDiskDate.isValid()) {
        QDBusInterface iface(navService, QStringLiteral("/disknav"), navInterface, bus);
        if (iface.isValid()) {
            const QDBusMessage reply =
                iface.call(QStringLiteral("ShowDisk"), focusDiskDate.toString(Qt::ISODate));
            if (reply.type() != QDBusMessage::ErrorMessage)
                return 0; // 已转交运行中的实例
        }
    }
    if (navRegistered) {
        auto *nav = new DiskNavObject(&app);
        bus.registerObject(QStringLiteral("/disknav"), navInterface, nav,
                           QDBusConnection::ExportAllSlots);
    }

    // 单实例。
    // ⚠️ 只能调用一次：DTK 明确拒绝重复调用（实测日志
    //   "call setSingleInstance again within the same process"），
    //   且失败时 QLocalServer::listen 会以「未知错误 22」失败。
    //   ⚠️ 依赖 XDG_RUNTIME_DIR 指向**真实存在**的目录，否则 listen 直接失败
    //   （表现：应用秒退，误判成崩溃）。
    if (!app.setSingleInstance("org.deepin.panshi-manager")) {
        qWarning() << "Another instance is running, exit.";
        return 0;
    }

    // 主题默认跟随系统: 不强制任何主题 (DTK 默认 themeType=0 跟随系统外观，
    // 并尊重用户在系统设置中对本应用的单独深浅设置)
    // QSS 颜色取自 applicationPalette，主题切换时重新生成以保持跟随
    //
    // 挂 applicationPaletteChanged 而非 themeTypeChanged：后者发出时 DTK 的
    // 调色板替换**尚未完成**——实测在应用内「≡ → 主题」切深色的路径上，此刻
    // 重建 QSS 会把界面钉成「半新半旧」。applicationPaletteChanged 在调色板
    // 就位后才发，两个方向都稳。
    //
    // ⚠️ 不要再调 `qApp->setPalette()`（2026-10-05 暗色故障实测，DTK6 明令禁止）：
    //   DTK 会为此打出 "Don't use QGuiApplication::setPalette on DTK application."
    //   且副作用是**破坏性的**——`DGuiApplicationHelper::setPaletteType()` 之后
    //   DTK 不再把新调色板同步到 QApplication，于是：
    //     · 自绘组件走 applicationPalette() → 正常变暗；
    //     · DLabel / QLineEdit / QListView 等走 QApplication::palette() → 卡在亮色。
    //   两者错位 = 深色卡片底 #282828 配近黑文字 #000000（对比 1.4，字「消失」），
    //   侧栏整条停在白底 —— 即用户报告的现象。探针 panshi-palettetest 步骤 2/4
    //   逐步实测坐实：调一次之后连切回亮色都恢复不了（**不可逆**）。
    //
    // 现在只做三件 DTK 允许的事：
    //   ① 拆掉 QSS —— 让 QStyleSheetStyle 释放冻结的调色板快照；
    //   ② 按新调色板重建 QSS；
    //   ③ 重算落色标签 + 深度递归 update，让自绘组件与 DLabel 同步换色。
    QObject::connect(DGuiApplicationHelper::instance(),
                     &DGuiApplicationHelper::applicationPaletteChanged,
                     &app, [] {
        qApp->setStyleSheet(QString());
        qApp->setStyleSheet(panshiStyleSheet());

        // 落色标签必须重算：applySemantic 是把颜色快照进控件调色板的，
        // DLabel 收到 PaletteChange 会重走 initPainter() 覆盖掉它。
        // 不调这一句，亮→暗后所有文字仍是旧主题的近黑色。
        PanshiUi::refreshExactColorLabels();

        for (QWidget *w : QApplication::topLevelWidgets()) {
            w->style()->unpolish(w);
            w->style()->polish(w);
            // 深度递归 update()：顶层 w->update() 只刷新自身及已知脏区，
            // **隐藏页**（QStackedWidget 里未激活的那些）的自绘卡片不会被标脏
            // —— 它们在 paintEvent 里实时取 appPalette()，逻辑没问题，
            // 但**没人叫它们重画**，于是下次切到该页时仍是旧主题的像素。
            const auto updateAll = [](QWidget *root) {
                // ⚠️ 不用 qMakeUnique：那是 Qt5 API，Qt6 已移除（首次编译才暴露）
                QList<QWidget *> stack;
                stack.append(root);
                while (!stack.isEmpty()) {
                    QWidget *cur = stack.takeLast();
                    cur->update();
                    const auto kids = cur->findChildren<QWidget *>();
                    for (QWidget *k : kids)
                        stack.append(k);
                }
            };
            updateAll(w);
        }
    });
    app.setStyleSheet(panshiStyleSheet());


    MainWindow w;
    if (focusDiskDate.isValid())
        w.showDiskDate(focusDiskDate);
    w.show();

    app.setOOMScoreAdj(0);
    return app.exec();
}

#include "main.moc"
