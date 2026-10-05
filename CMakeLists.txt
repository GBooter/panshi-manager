cmake_minimum_required(VERSION 3.16)
project(panshi-manager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# LinguistTools：提供 qt_add_translations()（Qt 6.2+）。注意本 Qt 6.8.0 发行版
# 中该命令仍定义在 Qt6LinguistTools 包内（Qt6CoreMacros 不含），缺组件则
# configure 阶段报 Unknown CMake command（阶段 i18n C1 实测）。
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Svg DBus LinguistTools)
find_package(Dtk6Core REQUIRED)
find_package(Dtk6Gui REQUIRED)
find_package(Dtk6Widget REQUIRED)

# ---- 翻译（i18n）----------------------------------------------------------
# 约定（DTK 官方）：
#   * .qm 的 basename 必须与 DApplication::applicationName 一致（"panshi-manager"），
#     否则 DApplication::loadTranslator() 按 appName 查找时找不到。
#   * loadTranslator() 在标准翻译目录中按当前 locale 查找 .qm。
#     ⚠ 实测（2026-10-03 B3 系统级安装）：qt_add_translations() 的
#     RESOURCE_PREFIX 只把 .qm 编进 Qt 内嵌资源（":/i18n/"），**DTK 的
#     loadTranslator() 不读 Qt 资源、只搜文件系统**；且该命令不会自动生成
#     install 规则。干净系统上安装版因此找不到任何 .qm，英文静默回落中文。
#     修法：QM_FILES_OUTPUT_VARIABLE 取出生成的 .qm，显式 install 到
#     share/panshi-manager/translations/（= GenericDataLocation/appName/translations，
#     XDG_DATA_DIRS 内，与 install.sh 用户级 ~/.local/share 分支同构）。
#   * 源语言为中文（代码中的 tr() 源串即中文），故可选语言列表为 zh_CN + en_US。
#
# .ts 文件缺失时 qt_add_translations() 仍会正常工作（不生成该语言的 .qm），
# 因此首次导入时需要先跑一次 lupdate 生成 .ts（见 docs/开发文档.md §9）。
set(PANSHI_TS_FILES
    translations/panshi-manager_zh_CN.ts
    translations/panshi-manager_en_US.ts
)

set(PANSHI_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/backend.cpp
    src/pages/basepage.cpp
    src/pages/overviewpage.cpp
    src/pages/masterswitch.cpp
    src/pages/snapshotspage.cpp
    src/pages/versionpage.cpp
    src/pages/protectionpage.cpp
    src/pages/policypage.cpp
    src/pages/deployspage.cpp
    src/pages/updatepage.cpp
    src/pages/diskusagepage.cpp
    src/pages/aboutpage.cpp
    src/ui/components.cpp
    src/ui/confirmdialog.cpp
    src/ui/aboutdialog.cpp
    src/ui/settingindex.cpp
    src/ui/sidebar.cpp
    src/widgets/usagechart.cpp
    src/widgets/countdowndialog.cpp
    src/widgets/scanoverlay.cpp
    src/widgets/radarwidget.cpp
    src/widgets/checkoverlay.cpp
    src/disk/history.cpp
    src/disk/scanner.cpp
    src/disk/monitor.cpp
    src/disk/monitorconfig.cpp
    src/disk/monitormain.cpp
)

add_executable(panshi-manager ${PANSHI_SOURCES} data/icons/icons.qrc)

# ---- 配色探针 panshi-colorprobe（开发期专用，不安装）--------------------
# C7「使用率分档颜色」此前只能肉眼观察，且暗色复核受限于「本机系统级暗色不可达」。
# 探针在离屏下渲染真实 DLabel 并**采样实际像素色**，输出 JSON 供脚本断言；
# 进程内 setThemeType() 强制应用内主题，亮暗两套都能自动跑。
# 与页面共用 PanshiUi::usageSemanticForPct() / kUsageWarnPct —— 阈值单一定义点。
add_executable(panshi-colorprobe
    src/tools/colorprobe.cpp
    src/ui/components.cpp
)
target_include_directories(panshi-colorprobe PRIVATE src)
target_link_libraries(panshi-colorprobe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 系统保护页状态卡探针 panshi-protectionprobe（开发期专用，不安装）----
# 卡 3 的文案与语义色改过（2026-10-05：「重启后处理」+ 常态 Muted
# → 「重启后状态」+ 常态 Success）。而 components.h 的映射是
# `Success → Highlight`（跟随主题强调色）—— 颜色必须实测，不能靠「我以为」。
# 探针用**页面同一份映射**（src/pages/protectioncards.h）离屏渲染三分支，
# 采样主值文字的实际像素色，断言「常态色 ≠ Muted 灰」等（配色 / 文本落地 /
# 徽标药丸已绘制，每主题 12 条，亮暗各跑）。
add_executable(panshi-protectionprobe
    tools/probe_protection_card.cpp
    src/ui/components.cpp
)
target_include_directories(panshi-protectionprobe PRIVATE src)
target_link_libraries(panshi-protectionprobe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 暗色主题「深底黑字」根因探针 panshi-darkprobe（开发期专用，不安装）----
# 现象（2026-10-05 用户截图 + 像素实测）：切暗色后卡片底是深灰 #282828（对），
# 但 DLabel 文字仍是近黑 #060606（对比 1.3 = 看不见），侧栏底更是纯白 #ffffff 残留。
# 怀疑对象：main.cpp 里 qApp->setPalette()（DTK6 对它**明确报警告**）。
# 探针把「带 setPalette」与「不带 setPalette」两条路径离屏各渲一遍，采样同一批
# 控件（DLabel 走角色 / QLineEdit 走原生 QApplication QPalette）的实际像素对比度，
# 用**两条路径的差**判定根因，而不是靠读代码猜。
add_executable(panshi-darkprobe
    tools/probe_dark_theme.cpp
    src/ui/components.cpp
)
target_include_directories(panshi-darkprobe PRIVATE src)
target_link_libraries(panshi-darkprobe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 调色板冻结诊断 panshi-palettetest（开发期专用，不安装）------------
# 探针 v1 反直觉的实测结果：「带 setPalette」与「不带 setPalette」两条路径像素
# **完全相同**，且暗色下卡片底仍是白。说明 setPalette() 不是「钉住界面」，
# 而是让 setPaletteType() 整个失效。本探针把因果链一步步量出来：
# 全新进程 → setDark → 手动 setPalette → 切回 light → 再切 dark，
# 每步都记 applicationPalette() 与 QApplication::palette() 两侧的 Base/WindowText。
add_executable(panshi-palettetest
    tools/probe_palette_freeze.cpp
)
target_include_directories(panshi-palettetest PRIVATE src)
target_link_libraries(panshi-palettetest PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 暗色修复方案对比 panshi-darkfix（开发期专用，不安装）----------------
# 对比 4 个候选：现行 qApp::setPalette / 只清 QSS / setApplicationPalette / 两者合用。
# 每个方案跑「亮→暗→再亮」三轮，用真实控件离屏渲染 + 采样像素对比度，
# 同时检验**可逆性**（现行方案切回亮色后 Base 永久卡在暗色，已实测）。
add_executable(panshi-darkfix
    tools/probe_dark_fix.cpp
    src/ui/components.cpp
)
target_include_directories(panshi-darkfix PRIVATE src)
target_link_libraries(panshi-darkfix PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- DCI 解析诊断 panshi-dciquery（开发期专用，不安装）------------------
# 「图标看不清」要分清是**名字写错**（回落到单色）还是**图形本身弱**（换别的图标），
# 两者修法完全不同。直接问 DIconTheme：搜索路径有哪些、某名字在各主题下解析成哪个文件。
add_executable(panshi-dciquery
    src/tools/dciquery.cpp
)
target_include_directories(panshi-dciquery PRIVATE src)
target_link_libraries(panshi-dciquery PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 图标候选接触表 panshi-iconsheet（开发期专用，不安装）--------------
# 侧栏图标「看不清」不能靠猜图标名解决：DCI 名录有 1277 个 .dci，
# 哪些既有颜色锚点又语义对得上，只有渲染出来看才知道。
# 本工具按侧栏真实尺寸 20px + 3 倍放大，输出浅底/深底两张对照表。
add_executable(panshi-iconsheet
    src/tools/iconsheet.cpp
    data/icons/icons.qrc
)
target_include_directories(panshi-iconsheet PRIVATE src)
target_link_libraries(panshi-iconsheet PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 关于对话框渲染探针 panshi-aboutshot（开发期专用，不安装）----------
# 自绘「关于」是**接管 DAboutDialog 的内部布局**（清空后重铺），
# 最易翻车：清不干净留空壳 / showEvent 被基类覆盖 / 误删标题栏。
# 这些要等真机点开菜单才暴露，太迟 → 离屏渲染成图 + 结构断言。
add_executable(panshi-aboutshot
    src/tools/aboutshot.cpp
    src/ui/aboutdialog.cpp
    src/ui/components.cpp
)
target_include_directories(panshi-aboutshot PRIVATE src)
target_compile_definitions(panshi-aboutshot PRIVATE PANSHI_ICON_SVG="${CMAKE_CURRENT_SOURCE_DIR}/data/icons/panshi.svg")

target_link_libraries(panshi-aboutshot PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 侧栏渲染探针 panshi-navshot（开发期专用，不安装）------------------
# 侧栏图标「看不清」不能只靠「文件存在」判定：取回单色 symbolic、或 20px 下
# 糊掉，探针都可能全绿而真机一眼就看得出。本探针用**真实 PanshiSidebar**
# 离屏渲染成图，并把每个图标的饱和度实测值变成可回归的断言。
add_executable(panshi-navshot
    src/tools/navshot.cpp
    data/icons/icons.qrc
    src/ui/sidebar.cpp
    src/ui/components.cpp
    src/ui/settingindex.cpp
)
target_include_directories(panshi-navshot PRIVATE src)
target_link_libraries(panshi-navshot PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

target_include_directories(panshi-manager PRIVATE src)
target_compile_definitions(panshi-manager PRIVATE PANSHI_ICON_SVG="${CMAKE_CURRENT_SOURCE_DIR}/data/icons/panshi.svg")


target_link_libraries(panshi-manager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Svg
    Qt6::DBus
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 无障碍探针 panshi-a11y-probe（开发期专用，不安装）------------------
# O9 收尾：表格单元格级无障碍。finalizeTableAccessible() 此前只能靠肉眼确认，
# 且它有一条易踩的边界 ——「空字符串」与「值为 0 / 空格」必须区别对待，
# 写错时读屏会把有效数据念成「无内容」，肉眼却完全看不出问题。
# 探针在离屏下建真 QTableWidget，断言三条契约 + nullptr/空表不崩。
add_executable(panshi-a11y-probe
    tools/probe_table_a11y.cpp
    src/ui/components.cpp
)
target_include_directories(panshi-a11y-probe PRIVATE src)
target_link_libraries(panshi-a11y-probe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 动画开关探针 panshi-animprobe（开发期专用，不安装）----------------
# reduceMotionRequested() 原先用 DConfig 读 GSettings 的键
# （com.deepin.wrap.gnome.desktop.interface / enable-animations），
# 而该键只存在于 glib gschema，DConfig 目录里没有 → 功能一直静默失效。
# 探针三方对照：gsettings 真值 / DTK6 只读属性 HasAnimations / DConfig 旧路径，
# 并支持 PANSHI_ANIMPROBE_EXPECT=0|1 做自动化断言（改系统设置前后各跑一次）。
add_executable(panshi-animprobe
    tools/probe_anim.cpp
    src/ui/components.cpp
)
target_include_directories(panshi-animprobe PRIVATE src)
target_link_libraries(panshi-animprobe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# ---- 后端取值探针 panshi-backendprobe（开发期专用，不安装）--------------
# 总览页字段的取值来源是否正确，肉眼很难判：getDeployVersion() 曾取 deploy_list
# 的 first()（那是待生效的新部署，版本号为空），而「当前运行」要用 IsBooted 判定。
# 探针打印整个 deploy_list 并做三条断言，含反向对照（不得再等于 [0]）。
add_executable(panshi-backendprobe
    tools/probe_backend.cpp
    src/backend.cpp
)
target_include_directories(panshi-backendprobe PRIVATE src)
target_link_libraries(panshi-backendprobe PRIVATE
    Qt6::Core
    Qt6::Concurrent
)

# ---- 图标探针 panshi-iconprobe（开发期专用，不安装）--------------------
# 「没有图标」要分清断在哪一环：① desktop 的 Icon 名 ② XDG 主题能否解析
# ③ DTK 的 DIconTheme::findQIcon 能否解析（应用实际用这个）。
# ②③ 修法完全不同：② 通而 ③ 不通，通常意味着 DTK 读不到图标主题名。
add_executable(panshi-iconprobe
    tools/probe_icon.cpp
)
target_include_directories(panshi-iconprobe PRIVATE src)
target_link_libraries(panshi-iconprobe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Dtk6::Core
    Dtk6::Gui
)

# 翻译：声明 .ts 源文件 → 构建时 lrelease 生成 .qm → 自动安装
# （Qt6.2+ 提供；项目已全局开启 CMAKE_AUTOMOC/AUTORCC）
qt_add_translations(panshi-manager
    TS_FILES ${PANSHI_TS_FILES}
    LUPDATE_OPTIONS -no-obsolete
    # 不用 RESOURCE_PREFIX：Qt6.8 中它与 QM_FILES_OUTPUT_VARIABLE 互斥，
    # 且 DTK loadTranslator() 不读内嵌资源（详见上方实测注释）。
    QM_FILES_OUTPUT_VARIABLE PANSHI_QM_FILES
)

# 安装
install(TARGETS panshi-manager RUNTIME DESTINATION bin)
# .qm 显式安装：DTK loadTranslator() 只从文件系统查找（RESOURCE_PREFIX 的
# 内嵌资源不会被读取），系统级安装必须随包带上这两个文件。
install(FILES ${PANSHI_QM_FILES}
        DESTINATION share/panshi-manager/translations)
install(FILES data/icons/panshi.svg DESTINATION share/icons/hicolor/scalable/apps)
# desktop 入口：Exec 按实际安装前缀生成（磐石只读系统装 /usr/local，
# 打包镜像时装 /usr，写死任一绝对路径都会让另一形态的开始菜单失效）。
# GNUInstallDirs 给出 CMAKE_INSTALL_FULL_BINDIR（绝对路径）。
include(GNUInstallDirs)
set(PANSHI_BINDIR "${CMAKE_INSTALL_FULL_BINDIR}")
configure_file(org.deepin.panshi-manager.desktop
               ${CMAKE_CURRENT_BINARY_DIR}/org.deepin.panshi-manager.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.deepin.panshi-manager.desktop
        DESTINATION share/applications)

# 磁盘监控：DConfig schema + systemd user 单元
# ⚠️ DConfig schema 的安装位置与其它文件**不同**：必须是 <prefix>/share/dsg/configs/。
#
# 原因（2026-10-04 实测）：DSG 的配置查找路径**硬编码**为 /usr/share/dsg
# （DSG_DATA_DIRS 等环境变量一律不生效），系统自己的 schema 都在那里：
#   /usr/share/dsg/configs/{dde-launchpad,dde-printer,deepin-ai-daemon,...}
# 而本项目默认 prefix 是 /usr/local → 装到 /usr/local/share/dsg/configs，
# **DSG 根本不去那里找** → 运行时必然报：
#   Can't acquire config manager … Can't load resource: /org.deepin.panshi-manager
#   DConfig is invalid of appid=org.deepin.panshi-manager
# 且「磁盘监控」的启用状态/阈值读写全部退化为默认值（配置存不住）。
#
# 所以打包镜像必须用 -DCMAKE_INSTALL_PREFIX=/usr（desktop/systemd 同理）。
# 这里在配置期就检查并提示，避免「编译通过、一装上才发现配置不生效」。
if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    message(WARNING
        "PANSHI: CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}\n"
        "  DConfig schema 将装到 ${CMAKE_INSTALL_PREFIX}/share/dsg/configs/，\n"
        "  但 DSG 只扫描 /usr/share/dsg（路径硬编码，环境变量无效），\n"
        "  运行时 DConfig 会失效、磁盘监控配置读写退化为默认值。\n"
        "  打包时请用：cmake -DCMAKE_INSTALL_PREFIX=/usr ...")
endif()
install(FILES data/dsg/configs/org.deepin.panshi-manager/org.deepin.panshi-manager.json
        DESTINATION share/dsg/configs/org.deepin.panshi-manager)
# service 的 ExecStart 按实际前缀生成（与 desktop 同理：默认 /usr/local，
# 打包镜像可 -DCMAKE_INSTALL_PREFIX=/usr）。timer 无路径占位，直接原样安装。
configure_file(data/systemd/panshi-monitor.service
               ${CMAKE_CURRENT_BINARY_DIR}/panshi-monitor.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/panshi-monitor.service
              data/systemd/panshi-monitor.timer
        DESTINATION lib/systemd/user)
