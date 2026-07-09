# NppFTP 維護版

NppFTP 是 Notepad++ 的 FTP / FTPS / FTPES / SFTP 外掛。這個維護版目前重點放在三件事：

- 把已知高風險安全問題補起來。
- 整理 Windows 可重現 build 流程。
- 把遠端瀏覽器改成更適合日常工作的 PSPad-like 單層瀏覽模式。

原始專案資訊：

- 官方說明：[NppFTP](http://ashkulz.github.io/NppFTP/)
- 安裝與入門影片：[GitHub issue #305](https://github.com/ashkulz/NppFTP/issues/305)
- 原始建置文件：[BUILDING.md](https://github.com/ashkulz/NppFTP/blob/master/BUILDING.md)

## 為什麼改遠端瀏覽器

舊版 NppFTP 的遠端檔案視圖是樹狀結構。樹很完整，但 FTP 日常工作常見的情境其實是：

- 已經知道目前在 `/var/www/html` 之類的位置。
- 想快速搜尋目前資料夾裡的檔案。
- 想直接輸入路徑跳到常用目錄。
- 不想一次展開很多層目錄，也不想等整棵樹慢慢載。

所以這版改成接近 PSPad FTP 面板的設計：顯示目前位置、快速搜尋、可輸入或下拉的 Change dir，再用單層清單顯示目前目錄內容。

### 舊版樹狀瀏覽

![舊版 NppFTP 樹狀遠端瀏覽器](snapshot/orin.png)

舊版優點是結構清楚，但目錄一多就要一直展開、收合、捲動；如果伺服器目錄很深，操作會變得慢又累。

### PSPad 參考畫面

![PSPad FTP flat browser reference](snapshot/pspad.png)

PSPad 的 FTP 面板比較接近實際工作習慣：上方固定顯示目前路徑，下面只列目前目錄；要跳目錄時直接在 `Change dir` 輸入或從下拉選常用路徑。

### 新版 NppFTP 實作畫面

![新版 NppFTP PSPad-like flat remote browser](snapshot/new_notepad.png)

新版 NppFTP 保留必要資訊，改成目前目錄的單層清單：

- 上方顯示目前 FTP profile 與目前路徑。
- `Quick search` 可即時過濾目前目錄。
- `Change dir` 可以手動輸入路徑，也會保留最近使用的幾筆目錄。
- 清單顯示 `Name`、`Size`、`Modified`、`Type`、`Permissions`。
- 資料夾與檔案有圖示。
- 欄位標題可拖曳調整順序。
- 點入目錄或開檔時會有 wait cursor 回饋。
- `Change dir` 按 Enter 時，已知目錄會切換，已知檔案會開啟；未知路徑不做事，不跳錯誤。

這樣做不是為了少顯示資訊，而是讓常用操作變短：看目前在哪、搜尋目前資料夾、輸入路徑跳轉、打開檔案，都可以在同一個小面板內完成。

## 這輪維護做了什麼

安全與穩定性：

- 修正 FTPS hostname verification。
- 將 FTPS 憑證例外縮到 host/profile 範圍，不再用全域例外。
- 修正 cache path traversal，限制快取路徑不可逃出 cache root。
- 修正 FTP PASV response parsing overflow。
- FTP PASV data connection 預設連回 control peer，避免被惡意 PASV endpoint 帶走。
- 修正 `CUT_StrMethods::RemoveCRLF` unsigned underflow。
- 修正 SFTP directory listing path composition overflow。
- 預設 profile password/passphrase 改用 Windows DPAPI 儲存。
- GitHub Actions pin 到 commit SHA，並設定明確 workflow permissions。
- 增加 FTP multiline response 與 directory listing 上限，避免惡意伺服器造成 DoS。

建置與供應鏈：

- 新增 `build.bat` 與 `build_scripts.ps1`，整理目前 Windows x64 Release build 入口。
- `third_party_sources.md` 記錄第三方來源 URL、license、SHA256 與 fallback 策略。
- 目前不把第三方 tarball vendor 進 git；只有在需要離線或上游來源不穩時才自養 mirror。

遠端瀏覽器：

- 保留舊 tree code，先讓新 flat browser 可用再逐步替換。
- 新增目前路徑、快速搜尋、Change dir combo。
- 新增單層目錄清單、資料夾/檔案圖示、metadata 欄位與 header drag/drop。
- 支援 double-click 進目錄與下載開檔。
- 支援 typed path：已知目錄切換、已知檔案開啟、未知路徑 no-op。
- 修正 dock resize / splitter resize 後 flat browser 沒跟著重排的問題。

## Build

需求：

- Windows
- PowerShell 7.6+
- Visual Studio C++ tools
- CMake

快速建置：

```bat
build.bat
```

產物：

- Plugin DLL：`_build\Release\NppFTP.dll`
- Zip package：`_build\NppFTP-0.30.22-win64.zip`

`build_scripts.ps1` 會檢查 Visual Studio、CMake、Perl 等環境；OpenSSL / zlib / libssh 仍走既有 third-party build 流程，並保留 hash 驗證。

## 專案紀錄

- `history.md`：重要修正、踩雷、決策、build hash。
- `todo.md`：剩餘工作與不做項目。
- `third_party_sources.md`：第三方套件來源與 checksum。
- `docs/superpowers/plans/`：較大任務的分步計畫。
- `snapshot/`：README 使用的舊版 NppFTP、PSPad 參考、新版 NppFTP 畫面截圖。

## 目前狀態

已完成的主線：

- 安全加固第一輪。
- Windows baseline build。
- PSPad-like flat remote browser 第一輪。
- README 與維護紀錄整理。

還需要實機手動 QA：

- Notepad++ 內安裝 `_build\Release\NppFTP.dll`。
- 測 resize、icons、metadata columns、header drag/drop。
- 測 double-click 目錄/檔案。
- 測 `Change dir` 已知目錄、已知檔案、未知路徑 no-op。

## Build Status

[![Appveyor build status](https://ci.appveyor.com/api/projects/status/github/ashkulz/nppftp?branch=master&svg=true)](https://ci.appveyor.com/project/ashkulz/nppftp)
[![GitHub release](https://img.shields.io/github/release/ashkulz/NppFTP.svg)](https://github.com/ashkulz/NppFTP/releases)
