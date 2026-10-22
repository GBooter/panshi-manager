# UI 重构进度 · 阶段 2 外壳导航 + 确认入口统一

> 承接《UI-重构进度-阶段1b-QSS清理与图标迁移.md》。本轮改动基于阶段 1 验收通过的代码。

## 1. 确认入口统一（收尾阶段 1 遗留）

`common.h::confirmDialog` 不再持有自己的 DDialog 构造逻辑，改为薄封装委托给
`PanshiUi::askConfirm / askDanger`（src/ui/confirmdialog.cpp）。8 处调用点
（update×2、snapshots×4、protection×1、deploys×1）**零改动**，签名不变。

效果：全应用确认对话框实现只有 1 份（`confirmdialog.cpp`），孤儿组件消除；
`createDetailsBox`（错误详情折叠）随 askDanger 的使用链路激活。
后续页面级升级（摘要块 facts、后果清单 consequences）在阶段 3 逐页进行。

## 2. 阶段 2：侧边栏重构（方案 §3.1）

**新增 `src/ui/sidebar.h/.cpp`（PanshiUi::PanshiSidebar）**

- 实现：`QListView` + `QStandardItemModel` + 自绘 `QStyledItemDelegate`，
  无 Q_OBJECT（激活回调走 `std::function`），无 QSS、无 hex 色值
- 分组标题行（T7/TextTips，`Qt::NoItemFlags` → 键盘导航自动跳过）
- 导航项：16px 主题图标（首选名缺失→备选名→QStyle 标准图标三级回落）
- 选中态 = Highlight 柔和底（`softFill(Accent)`）+ Highlight 文字；
  hover = `ItemBackground`；圆角 = `Radius::Frame(8)`；无描边
- **键盘可达（修 I10）**：↑/↓ 移动、Enter 激活、Tab 可进入；
  焦点框由 `QStyle::PE_FrameFocusRect` 绘制（替换原 `NoFocus` + `outline:none`）

**MainWindow::initSidebar() 重写**

- 删除重复的「磐石设置 / 磐石系统管理」标题块（标题栏已承载）与
  「Deepin 25 · 基于 OSTree」脚注（技术信息归关于页）
- 分组：概览（总览）｜系统维护（磁盘空间、系统保护、备份与版本、系统更新）｜设置（策略与设置、关于）
- 侧栏宽度 210 → 200（方案 §3.4 标准档）
- `switchTo()` 改为 `m_navSidebar->setCurrentData()`（编程式选中不触发回调）

**命名对齐（§3.1 改名）**：磁盘占用→**磁盘空间**、备份管理→**备份与版本**、
备份策略→**策略与设置**。侧栏、页头、监控设置弹窗标题三处已同步
（`diskusagepage.cpp:456` 弹窗标题 → 「磁盘空间设置」）。

**style.h 清理**：删除 `#panshiNavItem` 整块（含 `:focus { outline: none }`）与
`#panshiAppTitle/#panshiAppSubtitle` 死规则。

## 3. 阶段 2：页头唯一化（修 L1）

- `VersionManagementPage`：页头标题改为「备份与版本」，页级操作集中到页头右侧——
  **刷新**（按当前 tab 分发）、**创建备份**（切到「系统版本」tab 时自动隐藏；
  经 `QMetaObject::invokeMethod(m_snapPage, "onCreate")` 触发私有槽，snapshotspage.h 零改动）
- `SnapshotsPage`：删除页头与刷新/创建备份按钮（约 -12 行）
- `DeploysPage`：删除页头与刷新按钮（约 -8 行）

## 4. 阶段 2 范围内刻意延后的 1 项

**搜索（§4.8）**：DSearchEdit 需要每个设置项注册关键词索引，依赖阶段 3 的
行式设置组件（PanshiRow）落地，否则索引没东西可挂。放到阶段 3/4 交界处实施。

## 5. 需编译侧验证的点

1. **`PanshiSidebar` 的键盘行为**：Tab 进入侧栏后 ↑/↓ 是否跳过分组行
   （依赖 QAbstractItemView 对 disabled 行的跳过行为，Qt6 应无问题）、Enter 是否触发 activated
2. **delegate 焦点框**：`PE_FrameFocusRect` 在亮/暗主题下是否可见（若太淡可改 Highlight 1px 圆角描边，需评审）
3. **图标回落链**：`security-high/changes-prevent`、`software-update-available/system-software-update`
   等 6 组图标名在 deepin 图标主题中的命中情况（运行时看侧栏是否出现通用回落图标即可判断）
4. **invokeMethod 私有槽**：`QMetaObject::invokeMethod(m_snapPage, "onCreate")` 需 onCreate 在 moc 注册
   （private slots 已满足）；若构建报找不到槽，把 snapshotspage.h 中 onCreate 移到 public slots 即可
5. **回归**：备份页双页头消失；切「系统版本」tab 时「创建备份」按钮隐藏；
   `gotoPage(PageId, SubPageId)` 子页跳转（还原流程入口）仍落在正确 tab

## 6. 下一步（阶段 3）

按既定顺序逐页重构：**关于 → 系统更新 → 策略与设置 → 系统保护 → 总览 → 备份与版本 → 磁盘空间**。
前 3 页不依赖 4.2 保护模型，可立即开始；每页独立提交、独立可回退。
