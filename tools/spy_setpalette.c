// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * spy_setpalette.c —— setPalette 调用方归因拦截器（诊断用，勿进产品）
 *
 * 用途：出现 "DGuiApplicationHelper: Don't use QGuiApplication::setPalette on
 *   DTK application." 警告时，用它证明**到底谁在调** QGuiApplication::setPalette。
 *   2026-10-05 用它定位到：该警告由 DTK 6.7.44 自身在 setPaletteType() 路径上触发，
 *   而非本项目代码（项目侧早已无此调用）。
 *
 * 三个必须知道的坑（都踩过）：
 *
 * 1) **mangled name 里的 const 编码**。实参 `const QPalette&` 编成 `RK8QPalette`
 *    （8 = pointer + const 的 "PT" 组合），**不是** RK7。写错的后果是
 *    **钩不到任何调用**，表现为「0 次捕获」—— 看起来像「没人调用」，
 *    **结论会完全反过来**。动手前先核对：
 *      nm -D <libQt6Gui.so.6> | grep QGuiApplication10setPalette
 *
 * 2) **必须同时钩 QApplication 版本**。`qApp->setPalette(...)` 的静态类型是
 *    QApplication*，实际调用 `_ZN12QApplication10setPaletteERK8QPalettePKc`
 *    （**多一个 `char const*` 参数**，即 disable 为 null），且它**不**转调
 *    QGuiApplication 版本 ⇒ 只钩后者会漏掉全部。
 *
 * 3) **不能 include 任何 Qt 头**。`#include <QPalette>` 会让本 .so 全局依赖
 *    libQt6Gui，于是**任何**程序（连 /bin/echo）预加载它都报
 *    `undefined symbol: qt_version_tag` → 退出码 127，整个测试作废。
 *    改用不透明前向声明即可（按引用传，不需要完整定义）。
 *
 * 另：成员函数签名含 `this`，拦截函数必须写**两参**（this + 引用）；
 * 单参会参数错位必崩。转调必须用 dlsym(RTLD_NEXT) 拿真函数。
 *
 * 构建 / 使用：
 *   g++ -shared -fPIC -O0 -g -o /tmp/spy.so spy_setpalette.c -ldl
 *   LD_PRELOAD=/tmp/spy.so ./build/panshi-manager
 * 栈里只给 `库名+偏移`，用 addr2line 解析：
 *   addr2line -f -C -e <库> <偏移>
 *
 * 冒烟（务必做，能挡住坑 3）：
 *   LD_PRELOAD=/tmp/spy.so /bin/echo ok     # 应正常输出 ok，不报 qt_version_tag
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

/* 不透明前向声明：避免 #include <QPalette>（见坑 3） */
typedef struct QPalette QPaletteOpaque;

static void dump_stack(void)
{
    void *buf[40];
    const int n = backtrace(buf, 40);
    for (int i = 1; i < n && i < 20; ++i) {
        Dl_info info;
        if (dladdr(buf[i], &info) && info.dli_fname) {
            const unsigned long off =
                (unsigned long)((char *)buf[i] - (char *)info.dli_fbase);
            fprintf(stderr, "  #%-2d %s+0x%lx\n", i, info.dli_fname, off);
        } else {
            fprintf(stderr, "  #%-2d %p\n", i, buf[i]);
        }
    }
    fprintf(stderr, "@@@@@@ 栈结束 @@@@@@\n");
    fflush(stderr);
}

/* ---- 入口 1：QGuiApplication::setPalette(const QPalette &) ---- */
extern "C" void spy_qgui_setPalette(void *self, const QPaletteOpaque *p)
    __asm__("_ZN23QGuiApplication10setPaletteERK8QPalette");

extern "C" void spy_qgui_setPalette(void *self, const QPaletteOpaque *p)
{
    static int inHook = 0;
    if (inHook)                     /* 防递归：dump 内部不用 fork，安全 */
        return;
    inHook = 1;
    static int seq = 0;
    fprintf(stderr, "\n@@@@@@ [QGuiApplication::setPalette #%d] @@@@@@\n", ++seq);
    dump_stack();

    typedef void (*Fn)(void *, const QPaletteOpaque *);
    static Fn real = NULL;
    if (!real)
        real = (Fn)dlsym(RTLD_NEXT, "_ZN23QGuiApplication10setPaletteERK8QPalette");
    if (real)
        real(self, p);
    else
        fprintf(stderr, "spy: dlsym 失败，未转调（界面可能不刷新）\n");
    inHook = 0;
}

/* ---- 入口 2：QApplication::setPalette(const QPalette &, const char * = nullptr) ---- */
extern "C" void spy_qapp_setPalette(void *self, const QPaletteOpaque *p, const char *cls)
    __asm__("_ZN12QApplication10setPaletteERK8QPalettePKc");

extern "C" void spy_qapp_setPalette(void *self, const QPaletteOpaque *p, const char *cls)
{
    static int inHook = 0;
    if (inHook)
        return;
    inHook = 1;
    static int seq = 0;
    fprintf(stderr, "\n@@@@@@ [QApplication::setPalette #%d] @@@@@@\n", ++seq);
    dump_stack();

    typedef void (*Fn)(void *, const QPaletteOpaque *, const char *);
    static Fn real = NULL;
    if (!real)
        real = (Fn)dlsym(RTLD_NEXT, "_ZN12QApplication10setPaletteERK8QPalettePKc");
    if (real)
        real(self, p, cls);
    else
        fprintf(stderr, "spy: dlsym 失败，未转调（界面可能不刷新）\n");
    inHook = 0;
}
