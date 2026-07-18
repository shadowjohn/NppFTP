# Remote List Navigation, Context Actions, and Sorting

## Goal

- flat remote list 有焦點時，按 Backspace 回到目前目錄的上一層。
- 任何目錄導航都先由伺服器確認可列出內容；失敗時保留原畫面並提示。
- 右鍵點目錄或檔案時，都能直接建立目錄與檔案，並依點擊目標決定建立位置。
- Name、Size、Modified、Type、Permissions 五個欄位都支援單欄升冪／降冪排序。

## Keyboard Navigation

- remote list 的鍵盤 gate 加入 Backspace，避免按鍵先被 Notepad++ 的 modeless dialog 處理吃掉。
- Backspace 不看目前選取的是檔案或目錄，一律導航到 `m_remoteCurrentDir->GetParent()`。
- 已在 root、未連線或沒有目前目錄時不執行任何操作。
- Backspace 沿用下節的 directory preflight，loading cursor 維持既有流程。

## Directory Entry Preflight

- 雙擊目錄、目錄按 Enter、Backspace 與 Change dir 都先送出 server `LIST`，不以 cached permission 字串猜測是否可進入。
- 導航開始時只記錄 pending target 並顯示 loading cursor；目前目錄、清單與選取項目暫時保持不變。
- 已載入的目錄也必須重新 `LIST`，因此可偵測連線期間被撤銷的權限。
- 未載入路徑沿用既有 `GetDirectoryHierarchy()`；已知路徑使用既有 `GetDirectory()`，兩者在成功通知後才呼叫 `SetRemoteCurrentDir()`。
- `QueueTypeDirectoryGet` 失敗時不得呼叫 `OnDirectoryRefresh()`，避免清空既有 cache 或把失敗的 pending target 當成成功導航。
- pending navigation 失敗時清除 pending target、停止 loading cursor、保留原畫面，並顯示 `Unable to enter directory` 提示。
- SFTP 沿用既有 failure kind，區分權限不足與路徑不存在；FTP／FTPS 若只有模糊 server rejection，顯示 generic rejection，不猜測原因。
- 非導航用途的 background refresh 失敗時同樣保留原 cache，但只沿用 Output 記錄，不新增 modal。

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
- focused navigation test 驗證 LIST 成功後才切換、失敗不 refresh、不清空 cache，且只對 pending navigation 顯示失敗提示。
- focused comparator test 驗證五欄升降冪、目錄優先、中文／英文名稱比較 tie-break，以及 `..` 不進入排序資料。
- focused target-selection test 驗證目錄、檔案與空白區的建立位置。
- 執行既有 focused tests 與 x64 Release build，確認 DLL 與 ZIP 正常產生。
