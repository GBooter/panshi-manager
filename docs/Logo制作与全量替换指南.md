# 应用图标（Logo）手动制作与全量替换指南

> **适用**：磐石设置（panshi-manager）· deepin/UOS V25 · Qt 6.8 / DTK 6.7
> **定位**：给你一张任意尺寸的源图，一步步做出「四角统一大圆角」的**全套软件图标**，并完成**项目内 + 打包 + 用户级部署**的全量替换。
> **首次落地**：2026-10-09（源图 `NewLogo/panshi.png` 1001×1001 → 现行 `data/icons/*`）
> **全部命令均在本仓库实机跑过**；文末 §8 是可逐条判定的验证清单。

---

## 0. 一句话流程

```
源图 → ① 归一到 1024 画布 → ② 四角统一大圆角（遮罩）→ ③ 导出 16~512 PNG
     → ④ SVG 封装（内嵌圆角位图）→ ⑤ 落位 data/icons/ → ⑥ 改三处安装规则
     → ⑦ 确认四处代码引用 → ⑧ 重编两目录 + 重打 deb + 部署用户级 → ⑨ 九项验证
```

**最终产出清单**（缺一个都算没换干净）：

| 文件 | 作用 |
|---|---|
| `NewLogo/panshi-rounded-1024.png` | 圆角主图（唯一真源，日后所有尺寸由它派生） |
| `data/icons/panshi.svg` | 应用图标本体；被 `icons.qrc` 内嵌进二进制 |
| `data/icons/panshi-{16,22,24,32,48,64,128,256,512}.png` | hicolor 标准尺寸（任务栏/ Dock / 文件管理器按此优先取） |
| （安装目标）`share/icons/hicolor/<N>x<N>/apps/panshi.png` | 打包/安装后的落地点 |

---

## 1. 设计规范（先定死参数，再动手）

| 项 | 规定值 | 理由 |
|---|---|---|
| 标准画布 | **1024×1024** | 覆盖最大展示场景；源图非正方形先 `-resize^ + -extent` 居中补方 |
| 圆角半径 R | **22.5% × 边长 = 230px**（1024 基准） | 与 deepin/macOS 主流大圆角观感一致；调参见 §9 |
| 四角处理 | **必须同半径**（`roundrectangle` 天然四角统一） | 需求硬约束；分别画四个角必出错 |
| 背景 | **透明 + 抗锯齿** | icon 主题在各底色上都成立；锯齿在非 2× 缩放时会闪 |
| 内容安全区 | 图形离边 ≥ 8%（≈82px） | 圆角已吃掉 22.5%，贴边会被切掉视觉焦点 |
| 小尺寸可读性 | 48px 以下建议**加粗描边/简化细节** | 摄影级位图缩到 16px 会糊成一团（本次实测 16px 尚可，见 §9） |

> ⚠️ **不要**先做完 512 再放大做 1024 —— 永远从最大画布向下缩放（`-resize`），否则所有尺寸都带插值模糊。

---

## 2. 工具准备

```bash
which convert magick rsvg-convert python3     # 前三个缺一不可
```

| 工具 | 用途 | 缺失时的替代 |
|---|---|---|
| `magick`（ImageMagick 7） | 缩放 / 圆角遮罩 / 尺寸导出 | 无。必须装（本文全部图片处理都基于它） |
| `rsvg-convert`（librsvg） | 先验算 SVG 能否被第三方渲染器正确解析 | 可跳过，但会失去一道校验 |
| `python3` | 把 PNG 转成 base64 内嵌进 SVG | 任何语言均可，仅做 base64 |
| Qt 的 `rcc` / `g++` | 验证「Qt 能不能渲染、圆角对不对」 | 强烈建议保留，这是唯一可信判据（§8.2） |

本项目 Qt 工具链位置（编译探针要用）：

```bash
cd <仓库根>
source .toolchain/env.sh
rcc=$(ls .toolchain/sysroot/usr/lib/qt6/libexec/rcc)   # 注意是 libexec 下，不是 bin
```

---

## 3. 第一步：源图 → 圆角主图

源图放 `NewLogo/panshi.png`，产出 `NewLogo/panshi-rounded-1024.png`：

```bash
cd <仓库根>
SIZE=1024
R=230          # 22.5% × 1024；改这里=改整体圆角风格（见 §9）

magick NewLogo/panshi.png \
  -resize ${SIZE}x${SIZE}^ -gravity center -extent ${SIZE}x${SIZE} -alpha set \
  \( -size ${SIZE}x${SIZE} xc:none -fill white \
     -draw "roundrectangle 0,0,$((SIZE-1)),$((SIZE-1)),$R,$R" \) \
  -compose DstIn -composite PNG32:NewLogo/panshi-rounded-1024.png
```

**命令要点**（改坏了多数在这里）：

- `-resize 1024x1024^` 的 `^` = 保证铺满，配合 `-gravity center -extent` 居中裁方；没有 `^` 会得到带透明边的图。
- `-alpha set` 必须先加. 源图是不透明 RGB，没有 alpha 通道的话遮罩无效、四角变黑块。
- `roundrectangle` 的起止用 `SIZE-1`（0 基坐标），直接用 `SIZE` 会让右下角少 1px。
- `PNG32:` 前缀强制带 alpha 输出，`PNG24` 会丢透明。

**验收**：立刻看一眼是否四角都圆、且四角一致。

```bash
magick NewLogo/panshi-rounded-1024.png -resize 400x400 /tmp/preview.png && echo "/tmp/preview.png"
magick NewLogo/panshi-rounded-1024.png \
  -format "左上角 alpha=%[fx:p{0,0}.a]（应为 0）\n" info:
```

---

## 4. 第二步：导出 hicolor 尺寸集

```bash
cd <仓库根>
mkdir -p .tmp/icons
for S in 16 22 24 32 48 64 128 256 512; do
    magick NewLogo/panshi-rounded-1024.png \
      -resize ${S}x${S} \
      -define png:compression-level=9 \
      .tmp/icons/panshi-${S}.png
done
```

尺寸说明（为什么是这九个）：`16/22/24/32/48/64` 覆盖任务栏、通知、标题栏；`128/256/512` 覆盖启动器大屏、关于页、HiDPI 取图。

**小尺寸锐化（可选）**：16/22/24 若糊，可单独加锐化再导出：

```bash
magick NewLogo/panshi-rounded-1024.png -resize 32x32 -unsharp 0x1+0.6+0.05 .tmp/icons/panshi-32.png
```

---

## 5. 第三步：SVG 封装（把圆角位图嵌进去）

**为什么位图 Logo 还要一个 SVG**：项目里 `panshi.svg` 是唯一被各种安装/打包规则引用的文件名（图标名 `panshi`、QSS 主题、.desktop 的 `Icon=panshi`）。位图无法无损矢量化时，**保留 SVG 文件名 + 内部嵌一张圆角 PNG** 是最省改动的做法 —— Qt 的 QSvg 支持 `data:image/png;base64`（已实测，见 §8.2）。

```python
# 存成 /tmp/mksvg.py 后 python3 /tmp/mksvg.py
import base64
png = open('.tmp/icons/panshi-512.png', 'rb').read()   # 嵌 512：体积与清晰度的平衡点
b64 = base64.b64encode(png).decode()
svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg width="128" height="128" viewBox="0 0 512 512"
     xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
  <image x="0" y="0" width="512" height="512" xlink:href="data:image/png;base64,{b64}"/>
</svg>
'''
open('.tmp/icons/panshi.svg', 'w').write(svg)
print('svg bytes:', len(svg))
```

三条约束：

- `width/height="128"` + `viewBox="0 0 512 512"`：默认渲染尺寸小，但可任意缩放；**两者要配套**，只改一个会导致 Renderer 按不同基准取图。
- 内嵌 512 而不是 1024：1024 版会让二进制体积 +2MB，而本应用最大展示只到 72~128px。
- 必须写 `xmlns:xlink` 并用 `xlink:href`；只写 `href` 的写法在旧 librsvg / QSvg 上取不到图。

第三方渲染器先验一遍（应输出到 stdout 之外的错误才说明 SVG 写坏了）：

```bash
rsvg-convert -w 256 -h 256 .tmp/icons/panshi.svg -o /tmp/svgcheck.png
magick /tmp/svgcheck.png -format "角=%[fx:p{0,0}.a] 中心=%[fx:p{128,128}.a]\n" info:
# 期望：角=0 中心=1
```

---

## 6. 第四步：落位 + 改三处安装规则

```bash
cd <仓库根>
cp .tmp/icons/panshi.svg    data/icons/panshi.svg
for S in 16 22 24 32 48 64 128 256 512; do
    cp .tmp/icons/panshi-${S}.png data/icons/panshi-${S}.png
done
```

> 替换前建议先备份：`cp data/icons/panshi.svg .tmp/logo-backup-<日期>/panshi-old.svg`

**必须同步的安装/打包规则**（漏一处 = 某一形态下还是旧图标）：

| 文件 | 位置 | 要加什么 |
|---|---|---|
| `CMakeLists.txt` | `install(FILES data/icons/panshi.svg ...)` 之后 | 九个尺寸 PNG 的 `install(...)`（`cmake --install` / debhelper 走这条） |
| `install.sh` | 用户级分支里 `cp data/icons/panshi.svg ...` 之后 | 同构的一遍拷贝循环到 `$HOME/.local/share/icons/hicolor/<N>x<N>/apps/` |
| `tools/build-deb.sh` | `mkdir -p "$STAGE.../scalable/apps"` 与 `install(...)` 之后 | 九个尺寸 `install` 进 `$STAGE` |

CMake 侧写法参考（与其他规则同风格，带注释）：

```cmake
# 各尺寸 PNG 随 SVG 一起安装：hicolor 主题按精确尺寸优先于 scalable 取图。
foreach(_s 16 22 24 32 48 64 128 256 512)
    install(FILES data/icons/panshi-${_s}.png
            DESTINATION share/icons/hicolor/${_s}x${_s}/apps)
endforeach()
```

> ⚠️ `data/icons/*.png` 不需要进 `icons.qrc` —— 只有 `panshi.svg` 需要被内嵌（它被代码直接引用）。

---

## 7. 第五步：代码里的四处引用（最容易漏的一环）

图标名 `panshi` 在代码里出现四处，**都必须改成「内嵌资源优先」**：

| 文件 | 位置 | 用途 |
|---|---|---|
| `src/mainwindow.cpp` | 构造里 `tb->setIcon(...)` | 标题栏图标 |
| `src/main.cpp` | `app.setProductIcon(...)` | 桌面环境的"关于"入口 |
| `src/pages/aboutpage.cpp` | `hero` 布局 72px 图标 | 关于页大图标 |
| `src/ui/aboutdialog.cpp` | `productIcon()` | DTK 自绘关于对话框 |

统一写法：

```cpp
QIcon appIcon(QStringLiteral(":/icons/panshi.svg"));   // 内嵌资源优先
if (appIcon.isNull())
    appIcon = DIconTheme::findQIcon(QStringLiteral("panshi"));   // 主题查找兜底
```

**为什么不能直接用 `DIconTheme::findQIcon("panshi")`**（2026-10-09 实踩）：
系统会优先解析到 **`/usr` 下残留的旧 `panshi.svg`**。只读系统（磐石）/usr 不可写、sudo 失效，那个旧文件**删不掉也覆盖不了** → 资源文件换了、界面却还是旧图标。改为内嵌优先即可完全绕开，且与项目 C9「自带 SVG 优先」的既有先例一致。

---

## 8. 验证清单（逐条可判定，跑完才算换完）

### 8.1 资源本身：四角必须透明且一致

```bash
for S in 16 22 24 32 48 64 128 256 512; do
  L=$((S-1))
  magick data/icons/panshi-${S}.png \
    -format "${S}px TL=%[fx:p{0,0}.a] TR=%[fx:p{${L},0}.a] BL=%[fx:p{0,${L}}.a] BR=%[fx:p{${L},${L}}.a]\n" info:
done
```

判据：**TL/TR/BL/BR 全为 0**（16px 允许 ≤0.02 的抗锯齿残留，四个角数值必须相同）。

> ⚠️ **别用 md5 判断两次生成的图是否一致**：ImageMagick 会在 PNG 里写 `date:create`/`date:modify`
> 的 tEXt 块，同一套命令跑两次 md5 必然不同（已实测：字节数相同、像素零差异，md5 却不同）。
> 要比对产物请用 `magick compare -metric AE a.png b.png null:`（输出 `0 (0)` 即完全一致）。

### 8.2 Qt 渲染验证（最关键，不可替代）

拿**应用真正的 qrc 资源**做渲染断言 —— Qt 能不能画出来、圆角在不在，只有这一条能证明：

```cpp
// /tmp/iconcheck.cpp
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QtGlobal>
#include <cstdio>
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QIcon ic(QStringLiteral(":/icons/panshi.svg"));
    if (ic.isNull()) { printf("FAIL: 内嵌资源为空\n"); return 1; }
    QImage img(128, 128, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img); ic.paint(&p, {0, 0, 128, 128}); p.end();
    auto a = [&](int x, int y) { return qAlpha(img.pixel(x, y)); };
    bool ok = a(1,1)<=8 && a(126,1)<=8 && a(1,126)<=8 && a(126,126)<=8
              && a(64,64)>=200;                      // 四角透明 + 中心不透明
    printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
```

```bash
cd <仓库根> && source .toolchain/env.sh
SR=$TC/sysroot
$SR/usr/lib/qt6/libexec/rcc data/icons/icons.qrc -o .tmp/iconcheck_qrc.cpp
g++ -std=c++17 -fPIC /tmp/iconcheck.cpp .tmp/iconcheck_qrc.cpp \
  -I$SR/usr/include/x86_64-linux-gnu/qt6 -I$SR/usr/include/x86_64-linux-gnu/qt6/QtCore \
  -I$SR/usr/include/x86_64-linux-gnu/qt6/QtGui \
  -L$SR/usr/lib/x86_64-linux-gnu -lQt6Core -lQt6Gui -o .tmp/iconcheck
QT_QPA_PLATFORM=offscreen QT_PLUGIN_PATH=$SR/usr/lib/x86_64-linux-gnu/qt6/plugins ./.tmp/iconcheck
```

### 8.3 端到端：关于页真的画出来了

```bash
source .toolchain/env.sh
export XDG_RUNTIME_DIR=$PWD/.tmp            # 必须是**存在的目录**，否则 QLocalServer 错误 22 秒退
QT_QPA_PLATFORM=offscreen ./build2/panshi-aboutshot
```

看输出的截图肉眼确认。**前提**：`panshi-aboutshot` 这个 target 必须链接 `data/icons/icons.qrc`（`CMakeLists.txt` 里补），否则探针里根本没有内嵌资源、验证结论不成立 —— 这个坑实测踩过一次。

### 8.4 打包与部署

```bash
bash tools/build-deb.sh            # 输出 >> md5sums OK 且含 9 个尺寸 PNG
dpkg-deb -c panshi-manager_*_amd64.deb | grep icons   # 确认 SVG + 9 个 PNG 都在包里
```

用户级部署（本机 /usr 只读时唯一有效手段，XDG 优先级高于系统目录）：

```bash
PREFIX=~/.local
cp data/icons/panshi.svg $PREFIX/share/icons/hicolor/scalable/apps/panshi.svg
for S in 16 22 24 32 48 64 128 256 512; do
  mkdir -p $PREFIX/share/icons/hicolor/${S}x${S}/apps
  cp data/icons/panshi-${S}.png $PREFIX/share/icons/hicolor/${S}x${S}/apps/panshi.png
done
gtk-update-icon-cache -f $PREFIX/share/icons/hicolor 2>/dev/null || true
```

### 8.5 两处一致别忘了

- `build/` 与 `build2/` **同源，都要重编**（用户习惯跑 `./build/panshi-manager`）。
- 加了新安装文件后 CMake 要重新 configure（`cmake -B build`），否则 install 规则没更新。

---

## 9. 调参与已知坑

**圆角风格调参表**（只改 `R`，其余步骤不动）：

| R（1024 基准） | 占比 | 观感 |
|---|---|---|
| 120 | 11.7% | 微软 Fluent 风，偏硬 |
| **230** | **22.5%** | **现行（推荐）**，deepin/macOS 大圆角 |
| 256 | 25% | 更圆润，接近超椭圆 |
| ≥400 | 39%+ | 往圆形靠，16px 下主体易被切，慎用 |

> 改 `R` 后**必须重跑 §4~§8**（尺寸、SVG、deb、验证全套），手工只改某一个尺寸会造成不同尺寸圆角不一致。

**已知坑（按踩到的代价排序）**：

| 现象 | 根因 | 处置 |
|---|---|---|
| 换了文件，界面还是旧图标 | `findQIcon("panshi")` 先命中 /usr 残留（只读系统删不掉） | 代码改「内嵌 `:/icons/panshi.svg` 优先」（§7） |
| 探针验证通过、真机却是旧的 | 探针 target 没链 `icons.qrc` | 给探针补 `data/icons/icons.qrc`（§8.3） |
| SVG 打开是空白/角不圆 | 缺 `xmlns:xlink` 或用 `href` 而非 `xlink:href` | 按 §5 的模板照抄 |
| 四角变黑块 | 忘了 `-alpha set`，RGB 源图无 alpha 通道 | 加 `-alpha set` + 输出 `PNG32:` |
| 16px 糊成一团 | 摄影级位图细节过多 | 为 16/22/24 单独做**简化重绘版**，或加描边提对比 |
| 深层 guerrilla：文件在，图标缓存不刷新 | 图标主题缓存 | `gtk-update-icon-cache -f`；重启会话最稳 |

**常见求助话语接一句免责**：`.desktop` 里 `Icon=panshi` 是**图标名**不是路径，改图标不需要动它；只有当改了图标名时，才需要同步改 desktop 与 `findQIcon()` 的名字。

---

## 10. 复算清单（打印贴墙）

- [ ] 源图已归一到 1024 并保持内容居方
- [ ] 圆角 R 已按 §9 选定，四角一致（像素校验 alpha=0）
- [ ] 九个尺寸 PNG 已导出，逐尺寸校验四角 alpha
- [ ] `panshi.svg` 已重生成（内嵌 512 圆角位图），`rsvg-convert` 先验通过
- [ ] `data/icons/` 已落位，旧图标已备份
- [ ] `CMakeLists.txt` / `install.sh` / `tools/build-deb.sh` 三处安装规则已同步
- [ ] 四处代码引用已改为「内嵌优先」
- [ ] Qt 渲染探针 PASS
- [ ] `panshi-aboutshot` 截图肉眼确认
- [ ] `build/` + `build2/` 重编通过，`build-deb.sh` 重打且 md5sums OK
- [ ] 用户级 `~/.local/share/icons/hicolor/` 已部署全套

---

**参考**：本次落地记录见 `.workbuddy/memory/2026-10-09.md`；Probe/验证件给的建设见 `.workbuddy/memory/MEMORY.md` §构建与部署。
