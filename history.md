# NppFTP 接續紀錄

## 2026-07-09 早上：初次理解專案

### 專案定位

- NppFTP 是 Notepad++ 的 Windows 外掛，提供 FTP、FTPS、FTPES、SFTP 連線、檔案瀏覽、下載、上傳、快取對映與佇列傳輸。
- 主要產物是 `NppFTP.dll`。程式是 Win32/C++，使用 Notepad++ plugin API、Win32 UI、TinyXML、UTCP、OpenSSL、zlib、libssh。
- `tinyxml/` 與 `UTCP/` 是 repo 內含原始碼；OpenSSL、zlib、libssh 由 `build_3rdparty.py` 下載並建置。

### 建置重點

- `CMakeLists.txt` 會先跑 `build_3rdparty.py` 建第三方相依，再把 `src/`、`src/Windows/`、`tinyxml/`、`UTCP/` 一起編成 module library。
- `Makefile.mingw` 支援 MinGW-w64，`BITS=32` 產 x86，`BITS=64` 產 x64。
- `BUILDING.md` 提到 Windows 可用 Visual Studio 2019+，Linux/Windows 可用 MinGW-w64；需要 Python 3、Perl、CMake。
- 目前 `build_3rdparty.py` 釘住：
  - OpenSSL 4.0.1
  - zlib 1.3.2
  - libssh 0.12.0
- GitHub Actions build matrix 包含 Windows 2022 的 `x64`、`ARM64`、`Win32`，以及 Ubuntu 24.04 cross-build。

### 主要入口

- `src/PluginInterface.cpp`
  - Notepad++ 外掛 callback 集中在這裡：`setInfo`、`getName`、`getFuncsArray`、`beNotified`、`messageProc`、`isUnicode`。
  - menu items 共 4 個：Show NppFTP Window、Focus NppFTP Window、About NppFTP、Upload Current File。
  - `NPPN_READY` 取得 Notepad++ plugin config dir 後呼叫 `nppFTP.Start(...)`。
  - `NPPN_SHUTDOWN` 呼叫 `nppFTP.Stop()`。
  - `NPPN_FILESAVED` 用 buffer id 取目前完整路徑，交給 `NppFTP::OnSave`。
  - `NPPN_BUFFERACTIVATED` 交給 `NppFTP::OnActivateLocalFile` 更新 UI 上傳狀態。

- `src/NppFTP.cpp`
  - `Start` 建立 `%Notepad++ plugin config%\NppFTP`，設定全域 `_ConfigPath` 與 `_HostsFile`。
  - 載入 `NppFTP.xml`、`Certificates.xml`，建立 `FTPWindow` 與 `FTPSession`。
  - `OnSave` 會從本機快取路徑解析 `username@hostname`，必要時自動連到符合的 profile，然後呼叫 `UploadFileCache`。
  - `SaveSettings` 會寫回 settings、profiles、certificates。

### Runtime 分層

- UI 層：`src/Windows/FTPWindow.*`
  - 管 dockable window、toolbar、treeview、queue window、output window、profile/settings dialogs、drag/drop。
  - 使用者點 toolbar/menu 後，多數操作交給 `FTPSession` 排入 queue。
  - queue 完成後，`FTPWindow::OnEvent` 依 operation type 更新 tree、打開下載檔、刷新遠端目錄或顯示錯誤。

- Session 層：`src/FTPSession.*`
  - 一個 session 有兩組 wrapper/queue：
    - `m_mainWrapper` + `m_mainQueue`：連線、目錄、rename、chmod、mkdir、delete 等操作。
    - `m_transferWrapper` + `m_transferQueue`：下載、上傳、copy 等傳輸。
  - `StartSession` 從 `FTPProfile::CreateWrapper` 建立 protocol wrapper，並設定 profile cache 的環境變數。
  - `Connect` 會 queue `QueueConnect`；如果 profile 有 NOOP interval，會建 timer queue。
  - `TerminateSession` 會確認未完成傳輸、刪 timer、清 queue、disconnect wrapper。

- Queue 層：`src/FTPQueue.*`、`src/QueueOperation.*`
  - `FTPQueue::Initialize` 會開 Win32 thread 跑 `QueueLoop`。
  - `QueueOperation::SendNotification` 透過 window message 通知 UI thread，跨 thread 時會等待 UI ack。
  - 同類且同目標的 operation 會用 `Equals` 去重。

- Protocol 層：`src/FTPClientWrapper*.cpp`
  - `FTPProfile::CreateWrapper` 依 security mode 建 wrapper：
    - `Mode_SFTP` -> `FTPClientWrapperSSH`
    - `Mode_FTP` / `Mode_FTPS` / `Mode_FTPES` -> `FTPClientWrapperSSL`
  - SFTP 走 libssh/libssh-sftp，known hosts 位置由 `_HostsFile` 控制。
  - FTP/FTPS/FTPES 走 UTCP/CUT_FTPClient，SSL certificate trust 存在 `Certificates.xml`。

- 設定與快取：`src/FTPSettings.*`、`src/FTPProfile.*`、`src/FTPCache.*`
  - Global cache 預設 `%CONFIGDIR%\Cache\%USERNAME%@%HOSTNAME%\%PORT%` 對應遠端 `/`。
  - Profile cache 會接到 global cache parent；查找是「profile map 由上到下先符合先用，最後才 global」。
  - 下載：遠端路徑 -> 本機 cache path；上傳：本機 cache path -> 遠端路徑。

### 改動時先看的檔案

- Notepad++ lifecycle/menu/notification：`src/PluginInterface.cpp`
- 外掛啟停、settings XML、存檔自動上傳：`src/NppFTP.cpp`
- 連線/session/queue 調度：`src/FTPSession.cpp`
- queue thread、notification ack：`src/FTPQueue.cpp`、`src/QueueOperation.cpp`
- protocol 實作：`src/FTPClientWrapperSSL.cpp`、`src/FTPClientWrapperSSH.cpp`
- profile/settings/cache：`src/FTPProfile.cpp`、`src/FTPSettings.cpp`、`src/FTPCache.cpp`
- UI 事件與 tree/queue/output：`src/Windows/FTPWindow.cpp`
- path/encoding helper：`src/PathUtils.cpp`、`src/StringUtils.cpp`

### 踩雷點

- 專案混用 `TCHAR`、UTF-8 XML、`CP_ACP` 轉換與手動配置字串；不要整檔重寫或任意改檔案編碼。
- 多數 path buffer 是 `MAX_PATH` 與 raw pointer，改 path/字串邏輯要小心長度、釋放責任與 NUL 結尾。
- queue notification 有跨 thread ack；不要在 UI thread 做會反等 worker 的長操作，容易卡住。
- `FTPCache::ClearCurrentCache` 會刪除目前映射到的 cache 目錄；改 cache map 前先確認不會誤刪使用者資料。
- `OnSave` 自動上傳依本機路徑中的 `username@hostname` 判斷 profile，若與目前連線不符會 terminate session。
- `TODO.txt` 既有提醒：timeout 可能造成 UI lock、printf 系列 buffer 不理想、TCHAR/CP_ACP 效率差、treeview drag、MSVC 下 ssh_userauth_none heap corruption。

### 本次狀態

- 只新增這份 `history.md`，沒有改程式碼。
- 尚未建置；本次是 repo 理解與接續紀錄。

## 2026-07-09 Codex Security 掃描紀錄

- 針對「是否有藏步」跑完 repository-wide standard security scan；未看到明確後門、隱性 C2 或刻意外連邏輯。
- 最終報告：`C:\Users\johnho\AppData\Local\Temp\codex-security-scans-gCSPkX\NppFTP\8b0e0e16846442ecc69978784eee5b9169e8726f_20260709T012657Z_qlmd309h\report.md`。
- 最終 10 筆 finding：high 1、medium 5、low 4。最高風險是 FTPS 憑證沒有做 hostname binding；其他重點是 SFTP/FTP parser memory safety、cache path traversal、PASV data endpoint 可被遠端指定、profile 密碼硬編碼 DES key、CI action mutable tag。
- 本次沒有 Windows C++ compiler，因此 memory corruption 類只做 static source trace，尚未動態 crash harness / exploit 驗證。
- `CAND-014` unqualified `UxTheme.dll` load 保留為 hardening note；沒有證明 Notepad++ 啟動路徑有可寫 DLL search-path 前置條件，所以不列正式 finding。
- 工作台 complete 時曾因掃描後出現的 untracked `snapshot/orin.png` 判定 worktree 變動而拒絕封存；已暫移到 scan dir 後完成封存，再原路復原該圖檔。

## 2026-07-09 接手 TODO

- 新增 `todo.md`，先做 third-party source ledger，再建立一條 baseline build，接著按風險修 security findings，最後做 PSPad-style remote browser。
- 第三方 tarball 先只記 URL/hash/source，不放進 git；只有離線或 reproducible release build 真的需要時才自養 mirror。

## 2026-07-09 第三方來源帳本

- 新增 `third_party_sources.md`：記錄 OpenSSL 4.0.1、zlib 1.3.2、libssh 0.12.0 的下載 URL/hash/license，也記錄 vendored TinyXML 與 UTCP 的 repo 子樹 digest。
- 2026-07-09 檢查下載來源：OpenSSL、zlib、libssh、TinyXML SourceForge 頁、UTCP SourceForge mirror 都可連；原 CodeProject UTCP 頁 HEAD 檢查失敗，先視為脆弱來源。
- 決策：目前不把第三方 tarball 放進 git；只有離線/reproducible release build 需要，或上游 archive 真的不穩時才自養 mirror。UTCP 要先補齊對應 license/source package 才能公開 mirror。

## 2026-07-09 Baseline build

- 新增 `build.bat` 與 `build_scripts.ps1`，本機 baseline 先固定走 Visual Studio x64 Release package，不同時處理 MinGW/Win32/ARM64。
- `.\build.bat -CheckOnly` 會檢查 VS C++ tools、CMake、cl、nmake、Perl；`.\build.bat` 會 configure 並 build `package` target。
- 可編譯環境：
  - PowerShell 7.6.2
  - Visual Studio Community 18.6.11822.322 / Visual Studio 18 2026 generator
  - MSVC 19.51.36246，toolset path `VC\Tools\MSVC\14.51.36231`
  - Windows SDK 10.0.26100.0
  - MSBuild 18.6.3+84d3e95b4
  - CMake 4.2.3-msvc3 from Visual Studio
  - Python 3.12.0 at `C:\Python312_x64\python.exe`
  - Strawberry Perl 5.42.2.1 portable under `_build\tools`, downloaded and SHA256-checked by `build_scripts.ps1`
- 踩雷：Git for Windows 的 Perl 5.42.2 缺 `Locale::Maketext::Simple`，OpenSSL `perl Configure` 會失敗；不能只檢查有 `perl.exe`。
- 踩雷：Strawberry Perl portable 的 `c\bin` 內也有 CMake 3.29.2，若放在 PATH 前面會搶走 VS CMake；腳本只把 `perl\bin` 放前面。
- 第一次 `.\build.bat` 成功：OpenSSL 4.0.1、zlib 1.3.2、libssh 0.12.0 都由 `build_3rdparty.py` 下載並顯示 `checksum OK`。
- 產物：
  - `_build\Release\NppFTP.dll`
  - `_build\NppFTP-0.30.22-win64.zip`
- 目前只有 warnings，沒有 build error。主要 warnings 是 UTCP 檔案在 code page 950 下出現 C4819、size/type conversion、OpenSSL 3+ deprecated DES、舊 Winsock API。依規則不要為了消 warning 任意轉檔案編碼。
- 第一次 OpenSSL `nmake` 很慢，會連 test/app/fuzz 目標也編；目前先不改 `build_3rdparty.py`，除非之後真的需要縮短 clean build 時間。

## 2026-07-09 FTPS hostname verification

- 修第一個 security finding：FTPS 憑證驗證原本只看 chain/手動接受憑證，沒有把 server certificate 綁到 profile hostname。
- 新增 `src/FTPSHostnameVerifier.h`，用 OpenSSL `X509_check_host` 檢查 DNS hostname，並用 `X509_check_ip_asc` 支援 IP SAN。
- `FtpSSLWrapper` 現在保存 expected hostname；`FTPClientWrapperSSL` 建立時把 profile hostname 傳入。`OnSSLCertificate` 只有在 chain valid 或既有例外命中，且 hostname match 時才靜默接受。
- CA-valid wrong-host certificate 現在會進入既有 FTPS certificate warning flow，不再直接視為 valid。使用者當次仍可手動接受；永久例外仍未 host/profile scoped，留給下一個 TODO。
- 新增 `tests/ftps_hostname_check.cpp` 作為最小 regression check，驗證 DNS SAN/IP SAN 的 match 與 mismatch。
- 驗證：
  - 先編譯測試時 helper header 不存在，`tests\ftps_hostname_check.cpp(1): fatal error C1083`，確認紅燈有效。
  - 補 helper 後，用 MSVC 編譯並執行 `_build\tests\ftps_hostname_check.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；仍有既有 warnings，沒有 error。

## 2026-07-09 FTPS certificate exception scope

- 修第二個 security finding：使用者接受過的 FTPS invalid certificate 以前只用 DER 全域記錄，任何 profile/host/port/security mode 都可能重用同一張例外。
- 新增 `src/FTPSCertificateScope.h`，scope 必須同時符合 `profileName`、`profileParent`、`hostname`、`port`、`securityMode`。
- `SSLCertificates` 新增 scoped X509 store，`Certificates.xml` 新格式會把 DER 與 scope 欄位一起保存；舊的無 scope `Certificate DER="..."` 條目不再自動載入成 trusted exception，使用者需要在正確 profile/host 下重新接受。
- `FtpSSLWrapper::OnLoadCertificates` 現在只把同 scope 的例外加入 OpenSSL trust store；`OnSSLCertificate` 判斷 previously accepted 時也同樣檢查 scope 與 hostname。
- `FTPProfile::CreateWrapper` 會把 profile name/parent 傳給 FTPS wrapper；clone transfer wrapper 也會保留同一個 scope。
- 新增 `tests/ftps_certificate_scope.cpp` 作為最小 regression check，驗證 profile、parent、host、port、security mode 任一不同都不 match。
- 驗證：
  - 先編譯 scope 測試時 helper header 不存在，`tests\ftps_certificate_scope.cpp(1): fatal error C1083`，確認紅燈有效。
  - 補 helper 後，用 MSVC 編譯並執行 `_build\tests\ftps_certificate_scope.exe`，通過。
  - 重新編譯並執行 `_build\tests\ftps_hostname_check.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；仍有既有 warnings，沒有 error。

## 2026-07-09 Cache path traversal

- 修第三個 security finding：遠端路徑轉成本機 cache path 時，`../` 可經由 `PU::ConcatExternalToLocal` 逃出 cache root。
- 最小修補放在共用入口 `PU::ConcatExternalToLocal`，不在各 caller 分別補：拒絕 external path 裡的 `..` segment，canonicalize cache root 與結果路徑，並確認結果仍位於 root 底下。
- 新增 `tests/cache_path_traversal.cpp`，驗證正常 `folder/file.txt` 可轉換，`../escape.txt` 與 `folder/../../escape.txt` 會被拒絕。
- 驗證：
  - 修補前測試在 `../escape.txt` assert 失敗，確認紅燈有效。
  - 修補後用 MSVC 編譯並執行 `_build\tests\cache_path_traversal.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；仍有既有 warnings，沒有 error。

## 2026-07-09 FTP PASV parsing overflow

- 修第四個 security finding：PASV response 原本用 `strtok`/`atoi` 直接把 server 提供的六段數字組成 IP/port，沒有先限制每段 0..255，巨大數字或異常 token 會走未定義/錯誤路徑。
- 新增 `UTCP/include/ftp_pasv.h`，用 bounded integer parser 解析 `(h1,h2,h3,h4,p1,p2)`；六段都必須是純數字且 <=255，port 不能為 0。
- `UTCP/src/ftp_c.cpp` 四個 PASV 路徑現在共用 `CUT_ParsePASVResponse`，不再各自用 `strtok`/`atoi` 組字串。
- 依使用者指示，`UTCP/src/ftp_c.cpp` 本次改為 UTF-8 no BOM，後續可正常 patch；build 時該檔本身不再噴 C4819，剩下 C4819 來自它 include 的 UTCP header。
- 新增 `tests/pasv_response_parse.cpp`，驗證合法 PASV 可解析，`256`、巨大數字、缺欄位、port 0、非數字 token 都會被拒絕。
- 驗證：
  - 先編譯測試時 helper header 不存在，`tests\pasv_response_parse.cpp(1): fatal error C1083`，確認紅燈有效。
  - 補 helper 後，用 MSVC 編譯並執行 `_build\tests\pasv_response_parse.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`；仍有 UTCP header 的 C4819 warnings，沒有 error。

## 2026-07-09 Git SSH key setup

- `origin` 是 `git@github.com:shadowjohn/NppFTP.git`；一開始一般 SSH push 失敗，錯誤是 `Permission denied (publickey)`。
- 已先用 GitHub CLI token 把 `codex/nppftp-security-maintenance` 推上 GitHub。
- 使用者提供的 PuTTY PPK 可用 `plink.exe` 成功通過 GitHub SSH 認證，回應 `Hi shadowjohn!`。
- 本機 repo 已設定 `.git/config` 的 `core.sshCommand` 走 PuTTY `plink.exe` 與該 PPK；這是 local config，不會進 commit。
- 驗證：`git ls-remote origin HEAD` 成功回 HEAD hash。
