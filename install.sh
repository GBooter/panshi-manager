#!/bin/bash
# 磐石设置 (DTK 版本) 安装脚本
# 自适应：/usr 可写 → 系统级安装；磐石只读根 → 用户级安装
set -e

cd "$(dirname "$0")"

echo "==> 构建..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# 检测 /usr 是否可写（磐石系统 /usr 为只读 overlay）
if [ -w /usr/bin ] && touch /usr/bin/.panshi-wtest 2>/dev/null; then
    rm -f /usr/bin/.panshi-wtest
    SYS_INSTALL=1
else
    SYS_INSTALL=0
fi

if [ "$SYS_INSTALL" = "1" ]; then
    echo "==> 系统级安装 (需要 root)"
    if [ "$(id -u)" != "0" ]; then
        echo "请以 root 身份运行: sudo ./install.sh"
        exit 1
    fi
    cmake --install build
    # 让当前用户会话认识新装的 systemd 用户单元。
    # ⚠️ 若本脚本经 sudo 运行，这里连的是 root 自己的用户实例（通常不存在）
    #    → 失败被 || true 吞掉，无副作用；此时由应用侧补做
    #    （DiskUsagePage 开启监控时会先 daemon-reload 再启用，见 debian/postinst 注释）。
    systemctl --user daemon-reload >/dev/null 2>&1 || true
    gtk-update-icon-cache -f /usr/share/icons/hicolor >/dev/null 2>&1 || true
    update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
    echo "==> 完成! 启动器搜索「磐石设置」或运行 panshi-manager"
else
    echo "==> 检测到只读系统 (/usr 只读)，使用用户级安装到 ~/.local"
    PREFIX="$HOME/.local"
    mkdir -p "$PREFIX/bin" \
             "$PREFIX/share/icons/hicolor/scalable/apps" \
             "$PREFIX/share/applications" \
             "$PREFIX/share/dsg/configs/org.deepin.panshi-manager" \
             "$PREFIX/share/panshi-manager/translations" \
             "$HOME/.config/systemd/user"

    cp build/panshi-manager "$PREFIX/bin/panshi-manager"
    cp data/icons/panshi.svg "$PREFIX/share/icons/hicolor/scalable/apps/panshi.svg"
    # desktop 入口：源文件 Exec 是 @PANSHI_BINDIR@ 占位符（供 cmake
    # configure_file 用），手工安装时替换成用户级 bin 绝对路径，否则
    # 开始菜单点击会落到不存在的 /usr/bin/panshi-manager。
    sed "s|@PANSHI_BINDIR@/panshi-manager|$PREFIX/bin/panshi-manager|" \
        org.deepin.panshi-manager.desktop > "$PREFIX/share/applications/org.deepin.panshi-manager.desktop"
    # ⚠️ DConfig schema 在用户级安装下**不会生效**：DSG 的查找路径硬编码为
    # /usr/share/dsg（环境变量与 ~/.local/share 一概不扫），只读系统上装不进去。
    # 仍然复制一份，仅为保留文件完整性、便于日后拷贝进 /usr；
    # 运行时 DConfig 会判定为不可用，MonitorConfig 自动降级到 QSettings
    # （~/.config/deepin/panshi-manager.conf），功能不受影响。
    cp data/dsg/configs/org.deepin.panshi-manager/org.deepin.panshi-manager.json \
       "$PREFIX/share/dsg/configs/org.deepin.panshi-manager/"

    # 翻译：.qm basename 必须与 DApplication::applicationName（panshi-manager）一致，
    # 否则 loadTranslator() 按 appName 查找到不到（静默不翻译）。
    # 安装位置与 CMake 的 qt_add_translations() 保持一致。
    # 未生成 .qm（未装 qt6-tools-dev-tools / 未跑 lupdate）时跳过，不影响主程序。
    if compgen -G "build/*.qm" >/dev/null 2>&1; then
        cp build/*.qm "$PREFIX/share/panshi-manager/translations/"
    else
        echo "    （未发现 build/*.qm，跳过翻译文件；安装 qt6-tools-dev-tools 后重新构建即可生成）"
    fi

    # systemd 用户单元：ExecStart 指向用户级二进制（源文件是
    # @PANSHI_BINDIR@ 占位符，与 desktop 同一套替换）
    sed "s|@PANSHI_BINDIR@/panshi-manager|$PREFIX/bin/panshi-manager|" \
        data/systemd/panshi-monitor.service > "$HOME/.config/systemd/user/panshi-monitor.service"
    cp data/systemd/panshi-monitor.timer "$HOME/.config/systemd/user/panshi-monitor.timer"
    systemctl --user daemon-reload >/dev/null 2>&1 || true

    # 注意：不执行 update-desktop-database / gtk-update-icon-cache！
    # Deepin 25 的 dde-application-manager 会自动监听 ~/.local/share/applications 变化，
    # 手动重建缓存会触发其全量 reload，与 dde-shell 同步竞态导致任务栏卡死。

    echo "==> 完成! 运行 panshi-manager（用户级安装，监控服务已配置）"
fi
