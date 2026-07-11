# Remote List Column Format

## Goal

- Size 欄位文字靠右。
- Modified 固定顯示為 `yyyy-MM-dd HH:mm:ss`，例如 `2026-07-11 21:01:00`。

## Design

- 建立遠端清單欄位時，只有 Size 使用 `LVCFMT_RIGHT`，其餘欄位維持靠左。
- Modified 保留現有 UTC 轉本地時間流程，再直接由 `SYSTEMTIME` 輸出固定格式，不受 Windows 地區格式影響。
- 不增加使用者設定或新的格式化框架。

## Verification

- 以一個小型測試鎖定 Modified 的固定輸出。
- 執行 x64 Release build，確認 DLL 與 ZIP 正常產生。
