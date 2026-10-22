#!/usr/bin/env python3
"""磐石设置 — 应用图标包内体积优化（商店 200KB 限制）。

背景（2026-10-21）：深度商店包格式检测要求「应用包内图标总大小 ≤ 200KB」，
优化前包内图标合计 ~507KB（SVG 179.5KB + 512/256/128 共 302KB + 小图 25KB），
被驳回。本脚本把总量压到 ~150KB，视觉主体无损。

策略（按文件）：
  * panshi.svg：内嵌位图由 512px@256色 换成 256px@64色（base64 后 ~35KB）。
    viewBox 保持 0 0 512 512，渲染时 2x 上采样；启动器/任务栏实际显示尺寸
    ≤128px，256px 源完全够用（原生分辨率肉眼验证通过）。
  * 512/256/128/64/48/32/24/22/16 PNG：对**现有文件**做调色板量化
    （FASTOCTREE + Floyd-Steinberg），不重新缩放——保持各尺寸已审定的像素，
     仅减色。色数表见 COLORS，大尺寸给足色数保画质，小尺寸（显示时也就
     十几像素）大胆减色。
  * 量化对主体（盾牌/硬盘/水晶/金边）零损失，仅背景渐变出现轻微抖动纹理，
     与原有的「城市光点」质感融合，图标场景可接受（已原生分辨率人眼验证）。

用法：
    python3 tools/optimize_icons.py            # 原地优化 data/icons/
    python3 tools/optimize_icons.py --dry-run  # 只报告不写文件
"""
import argparse
import base64
import io
import re
import shutil
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ICONS = ROOT / "data" / "icons"
BACKUP = ROOT / ".tmp" / "iconwork" / "orig"

# 各尺寸量化色数（None = 不量化，仅重编码）
COLORS = {
    "panshi-512.png": 64,
    "panshi-256.png": 128,
    "panshi-128.png": 96,
    "panshi-64.png": 64,
    "panshi-48.png": 64,
    "panshi-32.png": 48,
    "panshi-24.png": 48,
    "panshi-22.png": 48,
    "panshi-16.png": 32,
}

# SVG 内嵌位图：由 512 主图缩到 256 再量化
SVG_EMBED_SIZE = 256
SVG_EMBED_COLORS = 64


def quantized(img: Image.Image, colors: int) -> Image.Image:
    # ⚠ 必须先转 RGBA：P 模式（调色板+tRNS）图直接 quantize() 会重排调色板
    # 索引但不重排 tRNS → 大量像素指向半透明色 → 桌面渲染出透明洞
    # （2026-10-21 实测：中心区 19% 透明像素；且缩略图工具忽略 tRNS，
    #   肉眼预览看不出来，只有 rsvg/Qt 等真实渲染器才暴露）。
    if img.mode != "RGBA":
        img = img.convert("RGBA")
    return img.quantize(colors=colors,
                        method=Image.Quantize.FASTOCTREE,
                        dither=Image.Dither.FLOYDSTEINBERG)


def encode_png(img: Image.Image) -> bytes:
    buf = io.BytesIO()
    img.save(buf, "PNG", optimize=True)
    return buf.getvalue()


def optimize_pngs(dry: bool) -> int:
    total_before = total_after = 0
    for name, colors in COLORS.items():
        p = ICONS / name
        if not p.exists():
            print(f"  ★ 缺失 {name}", file=sys.stderr)
            continue
        before = p.stat().st_size
        img = Image.open(p)
        out = encode_png(quantized(img, colors) if colors else img)
        total_before += before
        total_after += len(out)
        if not dry:
            p.write_bytes(out)
        print(f"  {name}: {before:,} -> {len(out):,} bytes ({len(out)/before*100:.0f}%)")
    print(f"  PNG 小计: {total_before:,} -> {total_after:,} bytes")
    return total_before, total_after


def rebuild_svg(dry: bool) -> tuple:
    """用 256px@64色 位图替换 SVG 内嵌的 512px 位图。

    ⚠ <image> 元素的 width/height 必须保持 512 填满 viewBox（0 0 512 512）：
    SVG 的 image 不会自动拉伸到画布，改成 256 会导致位图只占左上角 1/4
    （2026-10-21 实测翻车）。256px 位图由渲染器按 512 的 image 盒 2x 上采样。
    """
    svg_path = ICONS / "panshi.svg"
    svg = svg_path.read_text(encoding="utf-8")
    master = Image.open(ICONS / "panshi-512.png")  # 已是量化后的 512 主图
    if master.mode != "RGBA":                      # P 模式 LANCZOS 会插值调色板索引
        master = master.convert("RGBA")
    small = master.resize((SVG_EMBED_SIZE, SVG_EMBED_SIZE), Image.LANCZOS)
    png = encode_png(quantized(small, SVG_EMBED_COLORS))
    b64 = base64.b64encode(png).decode("ascii")

    before = svg_path.stat().st_size
    new_svg, n = re.subn(
        r'base64,[A-Za-z0-9+/=]+',
        f'base64,{b64}',
        svg,
        count=1)
    if n != 1:
        raise SystemExit("SVG 中未找到内嵌 base64 位图")
    if not dry:
        svg_path.write_text(new_svg, encoding="utf-8")
    after = len(new_svg.encode("utf-8"))
    print(f"  panshi.svg: {before:,} -> {after:,} bytes "
          f"(内嵌位图 {len(png):,} -> base64 {len(b64):,})")
    return before, after


def verify_alpha() -> bool:
    """alpha 体检：各尺寸 PNG 中心区（避开四角圆角）应几乎全不透明。

    防止「P 模式直接 quantize → tRNS 错乱 → 透明洞」这类问题静默通过
    （缩略图工具忽略 tRNS 看不出来，桌面真实渲染器才会暴露）。
    """
    ok = True
    for name in COLORS:
        p = ICONS / name
        if not p.exists():
            continue
        im = Image.open(p).convert("RGBA")
        W, H = im.size
        px = im.load()
        bad = total = 0
        for y in range(H // 8, H * 7 // 8):
            for x in range(W // 8, W * 7 // 8):
                total += 1
                # 阈值 200：量化把 255 舍入到 245~249 属正常（视觉无差）；
                # P 模式 quantize 损坏会产生大量 <30 的透明洞，200 足以捕获。
                if px[x, y][3] < 200:
                    bad += 1
        ratio = bad / total * 100
        status = "PASS" if ratio < 0.5 else "FAIL"
        if ratio >= 0.5:
            ok = False
        print(f"  [{status}] {name}: 中心区非不透明像素 {ratio:.2f}%")
    return ok


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--backup", action="store_true",
                    help="先把原始图标备份到 .tmp/iconwork/orig/")
    args = ap.parse_args()

    if args.backup and not args.dry_run:
        BACKUP.mkdir(parents=True, exist_ok=True)
        for f in list(COLORS) + ["panshi.svg"]:
            src = ICONS / f
            if src.exists():
                shutil.copy2(src, BACKUP / f)
        print(f"已备份到 {BACKUP}")

    print("==> 量化各尺寸 PNG")
    optimize_pngs(args.dry_run)
    print("==> 重建 SVG 内嵌位图")
    rebuild_svg(args.dry_run)

    # 汇总：包内图标总大小（与 build-deb.sh staging 的文件集一致）
    names = list(COLORS) + ["panshi.svg"]
    total = sum((ICONS / n).stat().st_size for n in names if (ICONS / n).exists())
    print(f"\n包内图标总计: {total:,} bytes ({total/1024:.1f} KB)  "
          f"[商店限制 200KB = 204,800 bytes] -> {'PASS' if total <= 204800 else 'FAIL'}")

    print("==> alpha 体检（中心区应全不透明）")
    alpha_ok = verify_alpha()
    if not alpha_ok:
        print("  ★ alpha 体检未通过：量化破坏了透明度，禁止交付", file=sys.stderr)
        return 2
    return 0 if total <= 204800 else 1


if __name__ == "__main__":
    sys.exit(main())
