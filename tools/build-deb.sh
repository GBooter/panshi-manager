#!/bin/bash
# 磐石设置 (panshi-manager) — deb 打包脚本
#
# 用途：在无 debhelper / fakeroot / g++/cmake 的受限环境（只读容器）中，依据
#       CMakeLists.txt 的 install() 规则手工 staging，并用 dpkg-deb 直接打包。
#       产出的文件布局与维护脚本，等同于 debian/ 源包骨架在完整 deepin SDK 中
#       `dpkg-buildpackage -us -uc -b` 的结果。
#
# 规范：deepin 打包规范 — debhelper-compat 13 / source 3.0 (native) / DEP-5 copyright。
#       版本号**单一来源**为 debian/changelog（避免 changelog 与包名两处漂移）。
#       安装前缀固定为 /usr：DConfig schema 必须落在 /usr/share/dsg/configs
#       （DSG 查找路径硬编码，详见 CMakeLists.txt 内注释）。
#
# 前提：build/ 下已有编译产物（panshi-manager 二进制 + .qm 翻译）。
#       若需从源码重新编译，请在装有 Qt6/DTK6 工具链的 deepin 环境执行：
#           dpkg-buildpackage -us -uc -b
#
# 用法：bash tools/build-deb.sh
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# ---- 版本号 / 架构：单一来源取自 debian/changelog ----
VERSION="$(sed -n '1s/^[^(]*(\([^)]*\)).*/\1/p' debian/changelog)"
ARCH="$(dpkg --print-architecture)"
PREFIX=/usr
STAGE="$PROJECT_ROOT/_stage"
DEB="$PROJECT_ROOT/panshi-manager_${VERSION}_${ARCH}.deb"

# 运行时依赖（deepin V23 / Debian 12 包名）。dtk-codeviewer 等官方示例用
# ${shlibs:Depends} 由 debhelper 自动推导；此脚本环境无 debhelper，故显式声明，
# 且与二进制 readelf 的 NEEDED 列表一致（含 Qt SVG 插件所需的 libqt6svg6）。
DEPENDS="libc6, libgcc-s1, libstdc++6, libgl1, libqt6core6, libqt6gui6,
 libqt6widgets6, libqt6dbus6, libqt6svg6, libdtk6core, libdtk6gui, libdtk6widget"

echo "==> 版本号 ${VERSION}  架构 ${ARCH}  前缀 ${PREFIX}"

# ---- 前置条件校验（缺一项就中止，避免产出内容不全的“看似成功”的包）----
[ -n "$VERSION" ] || { echo "错误：无法从 debian/changelog 解析版本号" >&2; exit 1; }
command -v dpkg-deb >/dev/null || { echo "错误：未找到 dpkg-deb" >&2; exit 1; }
# 选 release 二进制：build/ 与 build2/ 中取体积较小者
# （debug 构建含 debug_info 明显更大；release 构建不含）
pick_bin() {
  local a="build/panshi-manager" b="build2/panshi-manager" chosen=""
  if   [ -f "$a" ] && [ -f "$b" ]; then
    if [ "$(stat -c%s "$a")" -le "$(stat -c%s "$b")" ]; then chosen="$a"; else chosen="$b"; fi
  elif [ -f "$a" ]; then chosen="$a"
  elif [ -f "$b" ]; then chosen="$b"
  else echo "错误：build/panshi-manager 不存在，请先编译" >&2; exit 1; fi
  echo "$chosen"
}
BIN="$(pick_bin)"
echo "==> 选用二进制: $BIN ($(stat -c%s "$BIN") 字节)"
for qm in translations/panshi-manager_en_US.qm translations/panshi-manager_zh_CN.qm; do
  [ -f "$qm" ] || { echo "错误：$qm 不存在，请先生成翻译" >&2; exit 1; }
done

# ---- staging ----
rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" \
         "$STAGE${PREFIX}/bin" \
         "$STAGE${PREFIX}/share/applications" \
         "$STAGE${PREFIX}/share/panshi-manager/translations" \
         "$STAGE${PREFIX}/share/icons/hicolor/scalable/apps" \
         "$STAGE${PREFIX}/share/dsg/configs/org.deepin.panshi-manager" \
         "$STAGE${PREFIX}/lib/systemd/user"

install -m 0755 build/panshi-manager "$STAGE${PREFIX}/bin/panshi-manager"
install -m 0644 build/*.qm           "$STAGE${PREFIX}/share/panshi-manager/translations/"
install -m 0644 data/icons/panshi.svg "$STAGE${PREFIX}/share/icons/hicolor/scalable/apps/"

# desktop / service 源含 @PANSHI_BINDIR@ 占位符（由 CMake configure_file 填）；
# 此处按 /usr 前缀替换 —— 写死绝对路径会让“deb 形态”或“用户级形态”其一失效。
sed "s|@PANSHI_BINDIR@/panshi-manager|${PREFIX}/bin/panshi-manager|" \
    org.deepin.panshi-manager.desktop > "$STAGE${PREFIX}/share/applications/org.deepin.panshi-manager.desktop"
sed "s|@PANSHI_BINDIR@/panshi-manager|${PREFIX}/bin/panshi-manager|" \
    data/systemd/panshi-monitor.service > "$STAGE${PREFIX}/lib/systemd/user/panshi-monitor.service"
install -m 0644 data/systemd/panshi-monitor.timer "$STAGE${PREFIX}/lib/systemd/user/"
install -m 0644 data/dsg/configs/org.deepin.panshi-manager/org.deepin.panshi-manager.json \
                "$STAGE${PREFIX}/share/dsg/configs/org.deepin.panshi-manager/"

# ---- 权限归位（工作区挂载的文件系统把所有源文件标成 0777，直接 cp -a 会带进包里，
#      lintian 会报 excessive-permissions；故统一：可执行 0755，其余 0644）----
chmod 0755 "$STAGE${PREFIX}/bin/panshi-manager"
find "$STAGE${PREFIX}" -type f ! -perm 0755 -exec chmod 0644 {} +
find "$STAGE${PREFIX}" -type d -exec chmod 0755 {} +

# ---- DEBIAN 控制区 ----
cat > "$STAGE/DEBIAN/control" <<EOF
Package: panshi-manager
Version: ${VERSION}
Section: admin
Priority: optional
Architecture: ${ARCH}
Depends: $(printf '%s' "$DEPENDS" | tr -d '\n' | tr -s ' ')
Maintainer: Deepin Packages Builder <packages@deepin.com>
Description: Deepin 25 磐石系统管理工具
 磐石设置（Panshi Manager）是面向 Deepin 25 不可变（immutable / ostree）系统的
 图形化管理工具，基于 DTK6 / Qt6 构建。
 .
 功能覆盖：总览、总开关、快照管理、版本与回滚、系统保护、策略与设置、
 部署、系统更新、磁盘空间监控等。内置磁盘空间守护进程（systemd 用户单元），
 通过 DConfig 持久化监控策略，并提供简体中文 / 英文界面。
EOF
install -m 0755 debian/postinst "$STAGE/DEBIAN/postinst"
install -m 0755 debian/postrm   "$STAGE/DEBIAN/postrm"
( cd "$STAGE" && find usr -type f -print0 | sort -z | xargs -0 md5sum > DEBIAN/md5sums )

# ---- 打包 ----
dpkg-deb --build --root-owner-group "$STAGE" "$DEB"

# ---- 独立验证（脚本不替自己背书）----
# 注：dpkg-deb -x 只解 usr/，不含 DEBIAN/ 控制区；
#     须用 dpkg-deb -e 单独抽出 DEBIAN 才能校验 md5sums。
echo "==> 验证：元数据 + 布局 + md5sums"
dpkg-deb -I "$DEB" | sed -n '1,40p'
dpkg-deb -c "$DEB"
_VD="$(mktemp -d)"; dpkg-deb -x "$DEB" "$_VD"; dpkg-deb -e "$DEB" "$_VD/DEBIAN"
( cd "$_VD" && md5sum -c DEBIAN/md5sums && echo ">> md5sums OK" ); rm -rf "$_VD"

echo "==> 已生成：$DEB"
ls -l "$DEB"
