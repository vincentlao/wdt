#include "Reporting.h"
#include <folly/String.h>

namespace facebook {
namespace wdt {

TransferStats& TransferStats::operator+=(const TransferStats& stats) {
  folly::RWSpinLock::WriteHolder writeLock(mutex_.get());
  folly::RWSpinLock::ReadHolder readLock(stats.mutex_.get());
  headerBytes_ += stats.headerBytes_;
  dataBytes_ += stats.dataBytes_;
  effectiveHeaderBytes_ += stats.effectiveHeaderBytes_;
  effectiveDataBytes_ += stats.effectiveDataBytes_;
  numFiles_ += stats.numFiles_;
  failedAttempts_ += stats.failedAttempts_;
  if (stats.errCode_ != OK) {
    if (errCode_ == OK) {
      // First error. Setting this as the error code
      errCode_ = stats.errCode_;
    } else if (stats.errCode_ != errCode_) {
      // Different error than the previous one. Setting error code as generic
      // ERROR
      errCode_ = ERROR;
    }
  }
  return *this;
}

std::ostream& operator<<(std::ostream& os, const TransferStats& stats) {
  folly::RWSpinLock::ReadHolder lock(stats.mutex_.get());
  const double kMbToB = 1024 * 1024;
  double headerOverhead = 0;
  size_t effectiveTotalBytes =
      stats.effectiveHeaderBytes_ + stats.effectiveDataBytes_;
  if (effectiveTotalBytes) {
    headerOverhead = 100.0 * stats.effectiveHeaderBytes_ / effectiveTotalBytes;
  }
  double failureOverhead = 0;
  size_t totalBytes = stats.headerBytes_ + stats.dataBytes_;
  if (totalBytes) {
    failureOverhead = 100.0 * (totalBytes - effectiveTotalBytes) / totalBytes;
  }
  os << "Transfer Status = " << kErrorToStr[stats.errCode_]
     << ". Number of files transferred = " << stats.numFiles_
     << ". Data Mbytes = " << stats.effectiveDataBytes_ / kMbToB
     << ". Header kBytes = " << stats.effectiveHeaderBytes_ / 1024. << " ("
     << headerOverhead << "% overhead)"
     << ". Total bytes = " << effectiveTotalBytes
     << ". Wasted bytes due to failure = " << totalBytes - effectiveTotalBytes
     << " (" << failureOverhead << "% overhead).";
  return os;
}

TransferReport::TransferReport(
    std::vector<TransferStats>& transferredSourceStats,
    std::vector<TransferStats>& failedSourceStats,
    std::vector<TransferStats>& threadStats)
    : transferredSourceStats_(std::move(transferredSourceStats)),
      failedSourceStats_(std::move(failedSourceStats)),
      threadStats_(std::move(threadStats)) {
  for (const auto& stats : threadStats) {
    summary_ += stats;
  }
  auto errCode = failedSourceStats_.empty() ? OK : ERROR;
  // Global status depends on failed files, not thread statuses
  summary_.setErrorCode(errCode);
}

std::ostream& operator<<(std::ostream& os, const TransferReport& report) {
  os << report.getSummary();
  if (!report.failedSourceStats_.empty()) {
    if (report.summary_.getNumFiles() == 0) {
      os << " All files failed.";
    } else {
      os << "\n"
         << "Failed files :\n";
      int numOfFilesToPrint =
          std::min(size_t(10), report.failedSourceStats_.size());
      for (int i = 0; i < numOfFilesToPrint; i++) {
        os << report.failedSourceStats_[i].getId() << "\n";
      }
      if (numOfFilesToPrint < report.failedSourceStats_.size()) {
        os << "more...";
      }
    }
  }
  return os;
}
}
}