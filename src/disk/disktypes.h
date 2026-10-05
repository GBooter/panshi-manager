// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

namespace Panshi {

// 单层目录/文件大小项
struct DiskItem {
    QString path;
    qint64  size = 0;
    bool    isDir = true;
};

// 高频轮询轻量样本
struct DiskSample {
    QDateTime ts;
    qint64    total = 0;
    QMap<QString, qint64> hotDirs;   // 已知增长源目录当前大小
};

// 某一天/某次采样的完整记录（逐日文件）
struct DiskDayRecord {
    QDate   date;
    qint64  capacity = 0;            // df 总容量
    qint64  total    = 0;            // df 已用
    QList<DiskItem> topLevel;        // / 下各一级目录大小（完整保留）
    QMap<QString, QList<DiskItem>> drillCache; // 增长目录下钻缓存
    QList<DiskSample> samples;       // 当日高频采样（支持日内回放）
    bool    complete = true;         // 完整扫描？(降级/排除时 false)
};

// 索引中的轻量摘要（时间线快速渲染用）
struct DiskDaySummary {
    QDate   date;
    QString file;                    // 对应逐日数据文件名
    qint64  capacity = 0;
    qint64  total    = 0;
    qint64  growth   = 0;
    bool    spike    = false;        // 是否飙升日
    QList<DiskItem> topGrow;         // 当日增长 Top 3（摘要级，供悬浮提示）
};

// 全局索引
struct DiskHistoryIndex {
    QList<DiskDaySummary> days;      // 按日期升序
    QDate firstDate, lastDate;
    qint64 storageBytes = 0;         // 当前占用
    qint64 budgetBytes  = 0;         // 预算
};

// 异常事件
struct DiskSpikeInfo {
    QDate   date;
    qint64  growth = 0;
    QString causeDir;                // 定位到的增长目录
    QList<DiskItem> causes;          // 增长贡献 Top 项
    bool    alerted = false;         // 是否已推送告警
};

// 指定时间范围汇总
struct DiskRangeStats {
    qint64 beginUsed = 0;
    qint64 endUsed   = 0;
    qint64 netGrowth = 0;
    qint64 dailyAvg  = 0;            // 日均增长
    qint64 maxDayGrowth = 0;
    QDate  maxDay;                   // 最大单日增长日期
    QDate  trendStart;               // 连续增长起始日
    qint64 etaDays = -1;             // 预计可写满天数（-1 趋稳）
    QList<DiskItem> topGrowItems;    // 区间增长 Top 项
};

} // namespace Panshi
