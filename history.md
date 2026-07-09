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

## 2026-07-09 FTP PASV endpoint policy

- 修第五個 security finding：PASV response 裡 server 回報的 host 不再直接拿來連 data socket，避免被導到非控制連線 peer 的位址。
- `CUT_ParsePASVEndpoint` 仍會解析並驗證 PASV 六段數字與 port，但 data host 一律使用控制連線已連上的 `m_szAddress`。
- `ResumeReceiveFilePASV`、`ReceiveFilePASV`、`SendFilePASV`、`GetDirInfoPASV` 都改走同一個 helper。
- 新增 regression：server 回 `10.0.0.5` 時，data host 仍應是控制 peer `203.0.113.9`。
- 驗證：
  - MSVC 編譯並執行 `_build\tests\pasv_response_parse.exe`，通過。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`8A142DA59B833D89FA4AB55622F2948D80DBEE71FDF41690380955DFD0476539`。
  - 仍有既有 UTCP header 的 C4819 warnings，沒有 error。

## 2026-07-09 RemoveCRLF unsigned underflow

- 修第六個 security finding：`CUT_StrMethods::RemoveCRLF` 對空字串會用 `size_t` 做 `len - 1`，造成 unsigned underflow 並讀到 buffer 前方。
- 同檔 `IsWithCRLF` 也有空字串讀 `buf[len-1]` 的同型問題，一併在同一個字串工具邊界補掉。
- `UTCP/src/utstrlst.cpp` 不是 UTF-8，`apply_patch` 無法讀；本次用 byte-for-byte Latin-1 roundtrip 只替換 ASCII 條件式，避免改檔案編碼。
- 新增 `tests/remove_crlf.cpp`，用 guard page 讓舊碼空字串 underflow 穩定重現。
- 驗證：
  - 修補前 `_build\tests\remove_crlf.exe` 回 `-1073741819` access violation，確認紅燈有效。
  - 修補後同一測試回 `remove_crlf_exit=0`。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`EEB862AEF1A59522D28EB98BBFEE17195696EAC7B99FC4E72421C3C7BCF8F614`。
  - 仍有既有 UTCP 編碼 C4819 warnings，沒有 error。

## 2026-07-09 SFTP directory listing path composition overflow

- 修第七個 security finding：SFTP `GetDir` 把 parent path 與 server 回傳的 filename 用 `strcpy`/`strcat` 寫入 `FTPFile::filePath[MAX_PATH+1]`，惡意 server 傳回超長 filename 會 overflow。
- `FTPClientWrapperSSH::GetDir` 三處修補：
  1. **Line 121–125**：`strcpy`+`strcat` 替換為 `snprintf`，組合路徑超出 buffer 時 skip 該 entry 並釋放 sftp_attributes。
  2. **Line 112**：`path[strlen(path)-1]` 對空字串會讀 `path[-1]`；改為先存 `pathlen`，用 `(pathlen > 0)` 守護。
  3. **Line 151**：`strncpy(file.mod, sfile->longname, sizeof(file.mod)-1)` 沒確保 NUL 結尾；補上 `file.mod[sizeof(file.mod)-1] = '\0'`。
- 同型修補 `FTPSession::GetDirectoryHierarchy`（line 274, 296）：`sprintf(currentPath, "%s%s/", currentPath, pathEntry)` 同時是 overlapping source/destination UB 與無 bounds check；改為 `snprintf` in-place append，超長時 early return 並釋放已分配的 `parentDirs`。
- 新增 `src/sftp_dir_path.h`，把 path composition 邏輯抽成可獨立測試的 inline helper。
- 新增 `tests/sftp_dir_path.cpp`，驗證正常路徑、超長 overflow 拒絕、邊界長度、空路徑、NULL 輸入。
- 驗證：
  - MSVC 編譯並執行 `_build\tests\sftp_dir_path.exe`，`sftp_dir_path_exit=0`，通過。
  - `.\build.bat` 通過，只重編 `FTPClientWrapperSSH.cpp` 與 `FTPSession.cpp`，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`6CAF3798B3C64F4D9FB0AB6124D8F8AFD04049D9E378696D3E81A7D15F508C0C`。
  - 仍有既有 UTCP 編碼 C4819 warnings，沒有 error。

## 2026-07-09 Pin GitHub Actions by commit SHA and set explicit workflow permissions

- 將 GitHub Actions 工作流（Workflow）使用的 Actions 全數改為使用完整的 40 字元 commit SHA 進行固定（Pinning），而非使用可變動的 version tags，並在註解中保留原始 Tag（如 `# v7`）以維持可讀性。
- 調整項目包含 `actions/checkout`、`actions/upload-artifact`、`softprops/action-gh-release` 及 `advanced-security/spdx-dependency-submission-action`。
- 安全加固工作流權限（Workflow Permissions）：
  - 於 `CI_build.yml` 頂層設定預設為最小權限 `permissions: contents: read`。
  - `build_windows` 與 `build_linux` 維持 read-only token；tag 發版改由獨立的 `release_artifacts` job 使用 `permissions: contents: write`。
  - `spdx_upload.yml` 維持原有的局部權限宣告並將 actions 釘死在對應的 commit SHA。

## 2026-07-09 Replace default profile password storage with Windows DPAPI or equivalent

- 修正第八個 security finding（預設設定檔密碼儲存弱點）：原本在未使用 Master Password 時，密碼與私鑰 Passphrase 是使用寫死的 DES 金鑰 `"NppFTP00"` 進行加密（等同於明文儲存）。
- 使用 Windows Data Protection API (DPAPI) 進行安全加固：
  - 於 `src/Encryption.cpp` 實作 `DPAPI_encrypt` 與 `DPAPI_decrypt` 輔助函式，內部呼叫 Win32 的 `CryptProtectData` 及 `CryptUnprotectData`。
  - 當沒有啟用 Master Password 且 `IsDefaultKey()` 為真時，新儲存的密碼/Passphrase 會以 DPAPI 加密，並在輸出的十六進位字串前綴 `"DPAPI:"` 識別。
  - 完美相容舊版設定檔（Backward Compatibility）：在解密時，若字串不包含 `"DPAPI:"` 前綴，會自動回退使用 DES 搭配預設金鑰 `"NppFTP00"` 進行解密，確保使用者更新插件後既有密碼不會遺失。
  - DPAPI 加密失敗時不再暗退回預設 DES；`FTPProfile::SaveProfile` 會回傳 `NULL` 讓儲存失敗，而不是寫入弱加密密碼。
  - 當使用者啟用 Master Password 時，系統會依舊自動切換為 DES 搭配 Master Password 加密模式。
- 新增 `tests/encryption_dpapi.cpp` 單元測試：
  - 測試預設模式下的 DPAPI 加解密（產生 `"DPAPI:"` 前綴）。
  - 測試舊版 DES 密碼的向後相容性解密。
  - 測試 Master Password 模式下切換回 DES 加解密之行為。
- 驗證：
  - 編譯並執行 `_build\tests\encryption_dpapi.exe`，`encryption_dpapi_exit=0` 通過所有 Assert 驗證。
  - 執行 `.\build.bat` 成功，產出新的 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`79FBCF76122C1B6BBB70FE3954C3F25CDF10F71B962669C2EEAB199CB8532E20`。

## 2026-07-09 Add caps for FTP multiline responses and directory listings

- 修正第十個 security finding（FTP 協定封包長度未限制弱點）：當連線至惡意或設定錯誤的 FTP 伺服器時，若伺服器發送無限的 FTP 多行回應（Multiline Response）或極大的目錄清單（Directory Listing），原本程式會無限循環讀取並分配記憶體，造成記憶體耗盡（OOM）或 CPU 飆高造成阻斷服務（DoS）攻擊。
- 限制資源消耗的加固措施：
  - 於 `UTCP/src/ftp_c.cpp` 定義最大限制常數：
    - `MAX_FTP_MULTILINE_RESPONSE_LINES = 10000` (控制連線多行回應最大限制 10,000 行)。
    - `MAX_FTP_DIR_INFO_LINES = 100000` (目錄清單檔案數量最大限制 100,000 筆)。
  - 於 `CUT_FTPClient::PeekResponseCode` 迴圈中加入行數計數與限制判定，超出時回傳 protocol failure，避免控制連線殘留資料後仍被當成成功連線。
  - 於 `CUT_FTPClient::GetDirInfo` 及 `CUT_FTPClient::GetDirInfoPASV` 接收目錄封包的迴圈中加入檔案數量計數與限制判定，超出時即關閉連線並回傳 `UTE_LIST_FAILED` 錯誤。
- 新增 `tests/ftp_response_cap.cpp` 單元測試：
  - 啟動一個本地端的 Mock FTP 伺服器執行緒。
  - 模擬伺服器發送 10,050 行的無限多行回應。
  - 驗證用戶端在讀取回應達 10,000 行上限後，連線會失敗關閉，不會把 capped greeting 當成成功。
- 驗證：
  - 編譯並執行 `_build\tests\ftp_response_cap.exe`，`ftp_response_cap_exit=0` 通過所有測試。
  - 執行 `.\build.bat` 成功，產出新的 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`1DA9917471C5DC237969EBFD069C92BE1012D6312AEB1BDB812956BFDE6A54C7`。

## 2026-07-09 Review fixups for external security pass

- 修正 review 發現的四個問題：
  - `CI_build.yml` 不再讓 untrusted build job 持有 `contents: write`；tag-only `release_artifacts` job 下載 artifacts 後再發 GitHub Release。
  - FTP multiline response cap hit 現在 fail closed；`client.Connect` 對超長 greeting 會回錯，不會把殘留 response 留在控制連線後續使用。
  - `FTPClientWrapperSSH::GetDir` 改用 `sftp_compose_entry_path`，避免測試 helper 和產品碼各跑一套。
  - DPAPI 預設加密失敗不再暗退回 DES；測試也移除加密與解密後密碼的 stdout 輸出。
- 額外補 `FTPProfile` 錯誤路徑：passphrase 解密失敗時不再 `strdup(NULL)`；profile password/passphrase 加密失敗時 `SaveProfile` 回傳 `NULL`。
- 額外補 `Encryption.cpp` 邊界處理：DPAPI 空字串 round-trip 有測到，`strlen()` 轉舊介面 `int` 前會先檢查 `INT_MAX`，避免新修補帶進 C4267 類型警告。
- 驗證：
  - `tests\ftp_response_cap.cpp` 修測試後先紅燈：舊碼回 `Connect result: 0` 並 assertion fail。
  - 修補後 `_build\tests\ftp_response_cap.exe` 回 `ftp_response_cap_exit=0`，輸出 `Connect result: 38`、`Multiline line count: 10000`。
  - `_build\tests\sftp_dir_path.exe` 回 `sftp_dir_path_exit=0`。
  - `_build\tests\encryption_dpapi.exe` 回 `encryption_dpapi_exit=0`，涵蓋 DPAPI 空字串、舊 DES 相容、Master Password 模式，且不再輸出密碼內容。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`56D0A89BFB65D4F75F4B47EC208E322BB18C705CC6D1455EDE4371851927047D`。

## 2026-07-09 PSPad-style flat remote browser first slice

- 依照 `snapshot/new.png` 的方向做第一版 current-directory browser：連線後隱藏原本樹狀視圖，改顯示目前 FTP host、目前路徑、quick search、change-dir combo，以及單層 list view。
- 保留舊 tree code 與 `FileObject` 資料流：profiles tree 斷線時照舊使用；連線時 tree 仍在背景接收 `OnDirectoryRefresh`，flat list 只顯示目前目錄 children，避免重接 FTP/SFTP queue。
- 新 flat list 行為：
  - 顯示 `..`、folders、files。
  - quick search 使用 case-insensitive substring filter。
  - change-dir combo 可手動輸入路徑並按 Enter；也會記住最近 8 筆本次執行期目錄。
  - double-click folder 進入該目錄；double-click file 沿用 `DownloadFileCache()` 下載後開檔。
  - toolbar 的 Open Directory 也改走同一條 `NavigateRemotePath()`，讓 flat view 能同步跳轉。
- 本次刻意不做：recent dirs 寫入 profile/settings、list context menu、drag/drop、folder/file icon。先讓單層瀏覽穩定可用。
- 新增 `src/remote_browser_utils.h` 與 `tests/remote_browser_utils.cpp`，只測 quick search filter 行為。
- 驗證：
  - 先跑紅燈：`tests\remote_browser_utils.cpp` 因缺少 `src/remote_browser_utils.h` 編譯失敗。
  - 補 helper 後 `_build\tests\remote_browser_utils.exe` 回 `remote_browser_utils_exit=0`。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`7D268F7846E479A8A11C7C9E05A2DF3E1C800ED97A9DE82CE48279846434F996`。

## 2026-07-09 Open tasks for flat remote browser polish

- 根據 Notepad++ 實測截圖與 PSPad 參考畫面，將下一刀拆成可獨立驗證的 task：resize/layout、icons、metadata columns、header drag/drop、busy cursor、typed path handling、spacing alignment、manual QA。
- 建立 `docs/superpowers/plans/2026-07-09-remote-browser-polish.md`，並同步更新 `todo.md` 的 `4.1 PSPad-style remote browser polish` 清單。
- 決策：先保留 source labels 為 ASCII，避免在這刀混入編碼/i18n 變更；若要中文 labels，另開 resource/i18n task。

## 2026-07-09 Fix flat browser resize layout

- 修正 PSPad-style flat remote browser 在 splitter 拖曳、放開、capture 變更後沒有同步重排的問題；每次 `WindowSplitter` 移動隱藏 tree panel 後，flat controls 會補跑 `LayoutRemoteBrowser()`。
- 補上 dock 高度過小時的 `clientheight` 下限，以及 search/change-dir/list column 的寬度下限，避免縮太小時把負尺寸丟給 `MoveWindow()` 或 list column。
- 決策：這刀不改 `WindowSplitter` 本體，降低影響舊 tree/queue layout 的風險；先把 flat browser 跟上既有 splitter 結果。
- 驗證：
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`5FDE4F9E6E84B87724E09F1D848883FD0EEA6F2E0453B5BA0CD9233AD9D843CF`。
  - Notepad++ 內的 shrink/enlarge/splitter/reconnect 手動 QA 尚未執行，留到完成 polish slice 後一起測。

## 2026-07-09 Add flat browser metadata columns

- 將 flat remote browser 的 list view 改為 PSPad 順序欄位：`Name`、`Size`、`Modified`、`Type`、`Permissions`，並開啟 `LVS_EX_HEADERDRAGDROP` 讓使用者可拖曳欄位順序。
- 資料列現在會從 `FileObject` 填入 metadata：檔案 size、local modified time、檔案 type、副檔名大寫、permissions/mod string；資料夾 size/type 留空。
- folder/file icon 重用既有 `TreeImageList`，新增只讀 `GetImageList()` 讓 flat list view 掛同一組 Windows small icons，避免另養 icon 資源。
- 新增 `remote_browser_format_size()`、`remote_browser_type_text()`、`remote_browser_permission_text()` helper，並補 `tests\remote_browser_utils.cpp` 測試。
- 驗證：
  - 先跑紅燈：`tests\remote_browser_utils.cpp` 因缺少 metadata helper 編譯失敗。
  - 補 helper 後 `_build\tests\remote_browser_utils.exe` 回 `remote_browser_utils_exit=0`。
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`FD57618A846554272A141B09DE6F9E9E630D04954571CAA2C2CD4BB2882235D8`。

## 2026-07-09 Show busy cursor for flat browser actions

- 新增 `m_remoteBusyCursor`，flat browser 進入資料夾、輸入路徑觸發 directory hierarchy、或開啟檔案下載快取時，會先把 remote controls 的游標切成 `IDC_WAIT`。
- `WM_SETCURSOR` 只在 remote labels/search/combo/list 及其 child control 上套用 wait cursor，splitter 游標仍優先。
- directory get / download queue operation 結束、connect/disconnect 收尾時會清掉 busy flag，避免游標卡住。
- 驗證：
  - `.\build.bat` 通過，產出 `_build\Release\NppFTP.dll` 與 `_build\NppFTP-0.30.22-win64.zip`。
  - `_build\NppFTP-0.30.22-win64.zip` SHA256：`0E9A2F883BD483CDDA09E4DABD5D3B5E72DD259C524DBDB21D44603A55B562E0`。
  - Win32 cursor 行為尚待 Notepad++ 內手動 QA 確認。
