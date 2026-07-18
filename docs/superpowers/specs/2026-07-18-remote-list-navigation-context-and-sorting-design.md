# Remote List Navigation, Context Actions, and Sorting

## Goal

- flat remote list 有焦點時，按 Backspace 回到目前目錄的上一層。
- 右鍵點目錄或檔案時，都能直接建立目錄與檔案，並依點擊目標決定建立位置。
- Name、Size、Modified、Type、Permissions 五個欄位都支援單欄升冪／降冪排序。

## Keyboard Navigation

- remote list 的鍵盤 gate 加入 Backspace，避免按鍵先被 Notepad++ 的 modeless dialog 處理吃掉。
- Backspace 不看目前選取的是檔案或目錄，一律導航到 `m_remoteCurrentDir->GetParent()`。
- 已在 root、未連線或沒有目前目錄時不執行任何操作。
- 導航沿用 `SetRemoteCurrentDir(parent, true)`，因此 refresh 與 loading cursor 維持既有流程。

## Context Menu Create Target

- 目錄右鍵選單加入 `Create directory` 與 `New file`，建立目標為該目錄。
- 檔案右鍵選單加入相同兩項，建立目標為目前目錄。
- 空白區既有建立功能維持不變，目標仍為目前目錄。
- `..` 維持沒有操作選單，不提供建立、刪除或更名。
- 建立流程沿用既有 `CreateDirectory(FileObject*)` 與 `CreateFile(FileObject*)`，不新增第二套 FTP operation。

## Sorting

- 採用 view-only sorting：`FillRemoteList()` 只排序目前要顯示的 `FileObject*` 副本，不修改 `FileObject` children，也不改變舊 tree view 順序。
- 初始狀態沒有 active sort column；現有的目錄優先、名稱升冪順序維持不變。
- 第一次點任一欄位為升冪；再次點相同欄位切換升冪／降冪；點另一欄位時改成該欄升冪。
- header 只在 active column 顯示一個原生升冪或降冪箭頭。
- `..` 不參與排序，永遠固定在第一列。
- 目錄與檔案永遠分成兩組，目錄組固定在檔案組之前；降冪只反轉各組內的順序，不反轉兩組位置。
- Name 使用 Windows 使用者地區的 Unicode、不分大小寫比較；中文依目前 Windows 地區排序規則，不硬編碼筆畫或注音。
- Size 使用原始 byte 數值；目錄的 Size 畫面為空，因此目錄組以名稱作 tie-break。
- Modified 使用原始 `FILETIME` 比較。
- Type 使用畫面相同的 type／extension 文字比較；目錄的 Type 為空，因此目錄組以名稱作 tie-break。
- Permissions 使用畫面相同的 permission 文字比較。
- 所有 primary value 相同時，以 Name 作穩定 tie-break；tie-break 跟隨目前升冪／降冪方向。
- Quick search、refresh、切換目錄及 mutation refresh 重建清單時，持續套用目前 active sort。

## Scope

- 本刀只修改 flat remote list；不改舊 tree view 的鍵盤、右鍵或排序行為。
- 不新增排序設定持久化；plugin window 存活期間保留目前排序，重新載入 plugin 後回到初始狀態。
- 不新增 dependency 或自製國際化排序表。

## Verification

- focused Win32 ListView test 驗證 Backspace 能通過 modeless key gate。
- focused comparator test 驗證五欄升降冪、目錄優先、中文／英文名稱比較 tie-break，以及 `..` 不進入排序資料。
- focused target-selection test 驗證目錄、檔案與空白區的建立位置。
- 執行既有 focused tests 與 x64 Release build，確認 DLL 與 ZIP 正常產生。
