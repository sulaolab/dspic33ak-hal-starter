# PKOB4 Flash / Reset Tools

Command-line helper tools for flashing, resetting, and reading the UDID of a
dsPIC33AK512MPS512 Curiosity-class board through its on-board PKOB4 debugger.
They are intended for edit/build/flash/reset/observe loops without opening the
MPLAB X IDE.

The tools select the target board by **PKOB4 serial number**, so a specific board
can be addressed safely even when several PKOB4 boards are connected.

This directory contains three self-contained Windows x64 executables:

| Tool | Purpose |
| --- | --- |
| `flash_pkob4.exe` | Programs a HEX file. It can also run reset after a successful flash with `--reset-after-flash`. |
| `reset_pkob4.exe` | Reset only, using IPECMDBoost release-from-reset. It does not program flash. |
| `read_udid_pkob4.exe` | Reads the target device's 128-bit Unique Device ID. It does not program flash or reset intentionally, but connecting briefly drops the CDC console. |

The executables are published as self-contained .NET single-file apps, so a
separate .NET runtime is **not** required. MPLAB X must be installed; the tools
auto-detect the newest MPLAB X installation and use its bundled `mdb.bat`,
`ipecmdboost.jar`, and Java runtime.

Source is maintained in the `sulaolab/pkob4-flash-reset` repository
(private, MIT-0). Rebuild from source with `dotnet publish -c Release` under
`tools/flash_pkob4`, `tools/reset_pkob4`, or `tools/read_udid_pkob4`, then copy
the published executables into this directory.

---

## Board Serials

| Board | PKOB4 serial |
| --- | --- |
| Board A | `020085204RYN000318` |
| Board B | `020085204RYN001164` |
| Board C | `020085204RYN000057` |

Only operate on the board you intend to use. Do not use index-based selection
such as `hwtool ... -p <index>` when multiple boards are connected; connection
order can change. Use serial selection.

### List Connected Boards

Both `flash_pkob4.exe` and `reset_pkob4.exe` support `--list`. This is a USB
enumeration only; it is immediate and does not touch the target MCU.

```powershell
& "$dir\reset_pkob4.exe" --list
# Connected PKOB4 serial(s): 1
#   020085204RYN000057
```

`reset_pkob4.exe --list --probe` also connects to each board and reports the
target device name and Device Id. This **resets each probed board** and briefly
drops the CDC console. The default probe timeout is 60 seconds to allow cold
IPECMDBoost startup.

```powershell
& "$dir\reset_pkob4.exe" --list --probe
# Connected PKOB4: 1  (probing with device token '33AK512MPS512' -- this resets each board)
#   020085204RYN000057   dsPIC33AK512MPS512   Device Id 0xa77c
```

---

## Usage

From the repository root:

```powershell
$dir = Join-Path (Get-Location) 'buildtools\_flash_reset_tools'
```

### Flash

```powershell
$hex = 'C:\path\to\your.production.hex'

# Program Board C
& "$dir\flash_pkob4.exe" --serial 020085204RYN000057 --hex $hex --verbose

# Program, then reset after a successful flash
& "$dir\flash_pkob4.exe" --serial 020085204RYN000057 --hex $hex --reset-after-flash --verbose
```

Main options:

```text
--list            list connected PKOB4 serials and exit
--serial  <sn>    PKOB4 serial number (required)
--hex     <path>  HEX file to program (required)
--device  <dev>   device token (default dsPIC33AK512MPS512; MDB wants the long form)
--reset-after-flash
                  run reset_pkob4.exe for the same serial after a successful flash
--reset-device <dev>
                  reset_pkob4 device token; normally derived from --device
--timeout <sec>   per-attempt timeout in seconds (default 120)
--retry   <n>     retry count after failure (default 0)
--verbose         print detected paths, MDB script, output, and exit code
--dry-run         print what would run, without executing
-h, --help        show help
```

Internally, `flash_pkob4.exe` runs MPLAB X `mdb.bat` with a script equivalent to:

```text
device dsPIC33AK512MPS512
hwtool pkob4 -p <sn>020085204RYN000057
program "C:\path\to\your.production.hex"
quit
```

`--reset-after-flash` runs only after programming succeeds. If flashing fails,
the reset step is skipped.

### Reset

```powershell
& "$dir\reset_pkob4.exe" --serial 020085204RYN000057 --verbose
```

Main options:

```text
--list            list connected PKOB4 serials and exit
--probe           with --list: connect to each board and report device name + Device Id
                  (resets each board; confirms the --device token)
--check-java      side-effect-free IPECMDBoost warm/cold/stale state check
                  reports port 2012 owner and 2012.lock/ini; exit 0 = usable, 8 = stale/problem
--shutdown-boost  ask IPECMDBoost to quit via /OQ /OY2012, then exit
--clean-java      emergency cleanup: kill boost java, including port 2012 owner, and remove lock/ini
--serial  <sn>    PKOB4 serial number (required)
--device  <dev>   device token (default 33AK512MPS512; boost wants the short form)
--warm-timeout <sec>
                  timeout when boost java is already listening on port 2012 (default 5)
--cold-timeout <sec>
                  timeout for cold boost startup (default 60; 20+ seconds of silence can be normal)
--timeout <sec>   compatibility option: use the same timeout for warm and cold
--retry   <n>     recovery retry count after a warm failure (default 1)
--verbose         print detected paths, command, and exit code
--dry-run         print what would run, without executing
```

`reset_pkob4.exe` keeps a healthy IPECMDBoost Java server alive after successful
resets. This makes the next reset fast via the warm path. Cleanup is targeted:
when a timeout, decisive failure output, or non-zero exit occurs, the tool tries
official shutdown (`/OQ`), kills the Java process that owns port 2012 if still
alive, removes `2012.lock` / `2012.ini`, and retries cold once. If port 2012 is
free before a cold start but stale `2012.lock` / `2012.ini` files remain, they
are removed before starting boost.

Device token forms differ by tool:

- `flash_pkob4.exe` / MDB uses the long form: `dsPIC33AK512MPS512`
- `reset_pkob4.exe` / IPECMDBoost uses the short form: `33AK512MPS512`

Passing the long form to `reset_pkob4.exe` is rejected with a suggestion instead
of running into the confusing `PICDSPIC...` failure.

The reset operation pulses MCLR through IPECMDBoost release-from-reset. PKOB4 USB
re-enumerates, so a CDC console such as Tera Term briefly disconnects and must
reconnect. This is expected.

### Read UDID

```powershell
& "$dir\read_udid_pkob4.exe" --serial 020085204RYN000318
# UDID1=00D7794B
# UDID2=56080004
# UDID3=00EA010F
# UDID4=FFFFFFFF
# UDID128=FFFFFFFF00EA010F5608000400D7794B
# Serial: 020085204RYN000318
```

Main options:

```text
--list            list connected PKOB4 serials and exit
--serial  <sn>    PKOB4 serial number (required)
--device  <dev>   device token (default dsPIC33AK512MPS512; MDB wants the long form)
--timeout <sec>   per-attempt timeout in seconds (default 60)
--retry   <n>     retry count after failure (default 1)
--verbose         print detected paths, MDB script, output, and exit code
--dry-run         print what would run, without executing
```

`UDID128` is the concatenation of UDID4..UDID1, highest word first, matching the
firmware boot banner format. Use the full 128-bit value for board identity.

`read_udid_pkob4.exe` parses the `UDIDn = 0x...` lines that MPLAB X MDB prints
when connecting to the target. The documented `x /U4xw 0x7F2BE0` memory read path
does not work with the tested `dsPIC33AK-MP_DFP` 1.3.185 pack; it returns
garbage because `U` memory is not mapped for `x`. The connect-time output has
been verified against firmware reading the on-chip UDID addresses.

---

## Typical Flow

```powershell
$dir = Join-Path (Get-Location) 'buildtools\_flash_reset_tools'
$hex = 'C:\path\to\your.production.hex'

& "$dir\flash_pkob4.exe" --serial 020085204RYN000057 --hex $hex --verbose
& "$dir\reset_pkob4.exe"  --serial 020085204RYN000057 --verbose

# Or program and reset in one command
& "$dir\flash_pkob4.exe" --serial 020085204RYN000057 --hex $hex --reset-after-flash --verbose
```

The higher-level project wrapper `buildtools\flashauto.ps1` normally uses these
vendored executables automatically, so direct tool invocation is mostly useful
for diagnostics.

---

## Success Markers

Flash succeeds when MDB output includes:

```text
Target device dsPIC33AK512MPS512 found.
Programming/Verify complete
Program succeeded.
```

MDB may print a Java `ChronicleHashClosedException` during shutdown. If
`Program succeeded.` appeared before it, programming completed successfully.

Reset succeeds when output includes:

```text
PKOB4 Boost reset succeeded.
```

Reset exit codes:

```text
0  success
1  invalid arguments
2  MPLAB X / Java / IPECMDBoost not found
3  reset failed
4  timeout after recovery
5  unexpected exception
6  PKOB4 wedged; USB unplug/replug required
```

`flash_pkob4.exe --reset-after-flash` returns exit code 6 when flashing succeeded
but the reset step failed.

---

## Troubleshooting

- **MPLAB X IDE / IPE is open**: close it first. The IDE can hold the PKOB4.
- **Reset timeout / `Wait for current operation to complete`**: usually stale
  IPECMDBoost Java state or stale `2012.ini` / `2012.lock`. `reset_pkob4.exe`
  performs targeted cleanup automatically, but `--clean-java` is available as a
  manual escape hatch.
- **Exit 6 / `unloaded while still busy` / `unplug and reconnect`**: the PKOB4
  firmware is wedged. Host-side cleanup cannot recover this; unplug and reconnect
  the USB cable, then retry.
- **Wrong board selected**: always pass `--serial`. Use `--list` if unsure.

Manual cleanup, if needed:

```powershell
Get-CimInstance Win32_Process -Filter "Name='java.exe'" |
  Where-Object { $_.ExecutablePath -like '*MPLABX*' } |
  ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
Remove-Item "$env:USERPROFILE\.mchp_ipe\2012.ini" -Force -ErrorAction SilentlyContinue
```

---

## Notes

- These executables are build artifacts, approximately 68 MB each.
- They are vendored here so a fresh firmware repository clone can flash/reset
  without a separate install step.
- They do not require a separate .NET runtime. Rebuilding them from source does
  require the .NET 8 SDK.
- Update these vendored binaries by rebuilding the `sulaolab/pkob4-flash-reset`
  source projects and copying the published executables into this directory.

---

# Japanese

## PKOB4 Flash / Reset ツール

dsPIC33AK512MPS512 の Curiosity ボードを **コマンドライン**から素早く書き込み・リセットする
ためのツール一式です。MPLAB X IDE を開かずに使えます。PKOB4 の **シリアル番号でボードを
選択**するので、複数枚を接続したままでも狙った1枚だけを操作できます。スクリプトや AI
エージェントから build→flash→reset→観測ループを回すのに向いています。

このフォルダの 3 つの exe を用途で使い分けます（flash と reset はペア運用、または flash から自動 reset）:

| ツール | 役割 |
| --- | --- |
| `flash_pkob4.exe` | **書き込み**（HEX をプログラム）。`--reset-after-flash` で成功後に自動リセットも可能。 |
| `reset_pkob4.exe` | **リセット専用**（release-from-reset）。書き込みはしない。 |
| `read_udid_pkob4.exe` | **UDID 読み出し専用**（ダイ固有の 128bit Unique Device ID を表示）。書き込み・リセットはしないが、接続時に CDC コンソールは一瞬切れる。 |

どちらも .NET 不要の自己完結 exe（Windows x64、各 約 68 MB）です。
**ソースは GitHub リポジトリ `sulaolab/pkob4-flash-reset`（private, MIT-0）** で管理しており、
ローカルでは `tools/flash_pkob4/` と `tools/reset_pkob4/`（`dotnet publish -c Release` で再ビルド可）。

---

## ボードのシリアル番号

| ボード | PKOB4 シリアル |
| --- | --- |
| Board A | `020085204RYN000318` |
| Board B | `020085204RYN001164` |
| Board C | `020085204RYN000057` |

> 接続中の自分のボードだけを操作してください。他人が使っているボードには実行しないこと。
> （`hwtool ... -p <index>` のような番号指定は接続順で変わるので使わない。**必ずシリアルで指定**。）

### シリアルが分からない時は `--list`

どちらの exe でも `--list` で接続中の PKOB4 シリアルを一覧表示できます（USB 列挙のみ・**即時・無害**）:

```powershell
& "$dir\reset_pkob4.exe" --list      # flash_pkob4.exe --list でも可
# Connected PKOB4 serial(s): 1
#   020085204RYN000057
```

さらに `reset_pkob4 --list --probe` で各ボードの **デバイス名 + Device Id** も表示できます
（各ボードに接続するため **そのボードはリセットされ**、CDC コンソールが一瞬切れます）。
probe の既定 timeout は 60 秒です（コールド接続を見込む。成功時は即 return するので上限は実質無コスト）。

```powershell
& "$dir\reset_pkob4.exe" --list --probe
# Connected PKOB4: 1  (probing with device token '33AK512MPS512' -- this resets each board)
#   020085204RYN000057   dsPIC33AK512MPS512   Device Id 0xa77c
```

（従来どおり書き込みログの `INFO: <serial> successfully reserved` 行や MPLAB X の Tool 一覧でも確認できます。）

---

## 使い方

PowerShell から。リポジトリルートで実行する場合は次のように指定します。

```powershell
$dir = Join-Path (Get-Location) 'buildtools\_flash_reset_tools'
```

### 書き込み (flash)

```powershell
$hex = 'C:\path\to\your.production.hex'

# Board C に書き込み
& "$dir\flash_pkob4.exe" --serial 020085204RYN000057 --hex $hex --verbose

# 書き込み成功後に自動リセットまで行う
& "$dir\flash_pkob4.exe" --serial 020085204RYN000057 --hex $hex --reset-after-flash --verbose
```

主なオプション:

```text
--list            接続中の PKOB4 シリアル一覧を表示して終了（即時・無害）
--serial  <sn>    PKOB4 シリアル（必須）
--hex     <path>  書き込む HEX ファイル（必須）
--device  <dev>   デバイス名（既定 dsPIC33AK512MPS512 = mdb は「長い形」が正）
--reset-after-flash
                  書き込み成功後、同じ serial で reset_pkob4.exe を実行
--reset-device <dev>
                  reset_pkob4 用の短いデバイス名（通常は --device から自動導出）
--timeout <sec>   1 回あたりのタイムアウト秒（既定 120）
--retry   <n>     失敗時の再試行回数（既定 0）
--verbose         検出パス・mdb スクリプト・出力・終了コードを表示
--dry-run         実行せず、何をするかだけ表示
-h, --help        使い方
```

内部では mdb に次のスクリプトを渡しています（`<sn>` 接頭辞でシリアル選択）:

```text
device dsPIC33AK512MPS512
hwtool pkob4 -p <sn>020085204RYN000057
program "C:\path\to\your.production.hex"
quit
```

`--reset-after-flash` は書き込み成功時だけ実行されます。書き込みに失敗した場合は安全のため
リセットしません。reset 用デバイス名は `dsPIC33AK512MPS512` → `33AK512MPS512` のように自動変換します。

### リセット (reset)

```powershell
& "$dir\reset_pkob4.exe" --serial 020085204RYN000057 --verbose
```

主なオプション:

```text
--list            接続中の PKOB4 シリアル一覧を表示して終了（即時・無害）
--probe           --list と併用：各ボードに接続して device 名 + Device Id も表示
                  （各ボードをリセットする。--device の確認用）
--check-java      IPECMDBoost の warm/cold/stale 状態を即時判定（無害・ターゲット非接触）
                  port 2012 の所有 PID・lock/ini を表示。exit 0 = 使用可能、exit 8 = stale/problem
--shutdown-boost  IPECMDBoost に /OQ /OY2012 で公式終了を依頼して終了
--clean-java      緊急復旧用：boost java（port 2012 の所有者含む）を kill ＋ lock/ini 削除
--serial  <sn>    PKOB4 シリアル（必須）
--device  <dev>   デバイス名（既定 33AK512MPS512 = boost は「短い形」が正）
--warm-timeout <sec>
                  既に boost java が port 2012 で待機している場合の timeout（既定 5）
--cold-timeout <sec>
                  boost cold start の timeout（既定 60。初回は 20 秒以上無出力でも正常）
--timeout <sec>   warm/cold 両方へ同じ timeout を使う互換オプション
--retry   <n>     warm 失敗後の復旧リトライ回数（既定 1）
--verbose         検出パス・コマンド・終了コードを表示
--dry-run         実行せず、何をするかだけ表示
```

> **Boost は温存する方針です**：正常な `reset_pkob4` は IPECMDBoost の常駐 java を残します。
> これにより次回 reset は warm 経路になり高速です。timeout、明確な失敗出力、非ゼロ終了が出た場合だけ、
> `/OQ` → port 2012 owner java kill → `2012.lock|ini` 削除 → cold retry の順で復旧します。
> `2012.ini` や `2012.lock` は warm boost が生きている時にも存在し得るため削除しません。
> ただし port 2012 が空いている cold 起動前に残っている場合は stale とみなし、起動前に削除します。
> 手動で状態確認するには `--check-java`、公式終了だけ行うには `--shutdown-boost`、緊急掃除には `--clean-java`。

> **デバイス名の形に注意**：boost(reset) は **短い形 `33AK512MPS512`**、mdb(flash) は
> **長い形 `dsPIC33AK512MPS512`** が正で、ツールごとに逆です。reset に長い形を渡すと、
> `reset_pkob4` は短い形を提案して **exit 1 で止まります**（`PICDSPIC…` という分かりにくい
> 失敗を未然に防止）。通常は `--device` 省略（既定）で OK。

`reset_pkob4` は IPECMDBoost の release-from-reset を使います。MCLR をパルスする方式ですが、
USB が再列挙されるため **CDC コンソール（PRINTF / TeraTerm）は一瞬切れて再接続が必要**です
（これは正常動作）。

### UDID 読み出し (read_udid_pkob4)

ターゲット dsPIC33AK の **UDID（ダイ固有の 128bit ID）** を読み出します。UDID は
**PKOB4 シリアル（デバッガ ID）** や **DEVID（型番 ID）** とは別物で、**基板の個体識別**に使えます。

```powershell
& "$dir\read_udid_pkob4.exe" --serial 020085204RYN000318
# UDID1=00D7794B
# UDID2=56080004
# UDID3=00EA010F
# UDID4=FFFFFFFF
# UDID128=FFFFFFFF00EA010F5608000400D7794B
# Serial: 020085204RYN000318
```

主なオプション:

```text
--list            接続中の PKOB4 シリアル一覧を表示して終了（即時・無害）
--serial  <sn>    PKOB4 シリアル（必須）
--device  <dev>   デバイス名（既定 dsPIC33AK512MPS512 = mdb は「長い形」が正）
--timeout <sec>   1 回あたりのタイムアウト秒（既定 60）
--retry   <n>     失敗時の再試行回数（既定 1）
--verbose         検出パス・mdb スクリプト・出力・終了コードを表示
--dry-run         実行せず、何をするかだけ表示
```

`UDID128` は UDID4..UDID1 を上位 word から連結したもの（ファームの起動バナー表示と同形）。
同一ロットの 2 枚は UDID1/UDID2（lot/wafer）が一致し UDID3（ダイ X/Y 位置）で違うことが多いので、
判別は 128bit 全体で行うこと。

> **読み出し方式の注意**：`mdb` は**接続時に `UDIDn = 0x...` を自動出力**するので、それをパースします。
> ドキュメントにある `x /U4xw 0x7F2BE0` 方式は今回の DFP（`dsPIC33AK-MP_DFP` 1.3.185）では
> 機能せず（FF/00 のゴミを返す＝`U` メモリが `x` 用にマップされていない）、接続時出力が確実な経路です。
> 値はファームのオンチップ読み出し（`0x007F2BE0..EC`）と完全一致することを実機で確認済み。
>
> 接続はターゲットをリセットするため、開いている CDC コンソールは一瞬切れて再接続が必要です。
> MPLAB X IDE / IPE が同じ PKOB4 を掴んでいると失敗します（先に閉じること）。

### 典型的な流れ（ビルド済み HEX を焼いて動かす）

```powershell
$dir = Join-Path (Get-Location) 'buildtools\_flash_reset_tools'
$hex = 'C:\path\to\your.production.hex'
& "$dir\flash_pkob4.exe" --serial 020085204RYN000057 --hex $hex --verbose
& "$dir\reset_pkob4.exe"  --serial 020085204RYN000057 --verbose

# または、書き込み成功後に自動リセット
& "$dir\flash_pkob4.exe" --serial 020085204RYN000057 --hex $hex --reset-after-flash --verbose
```

---

## 成功の見分け方

**flash** が成功すると mdb 出力に次が出ます:

```text
Target device dsPIC33AK512MPS512 found.
Programming/Verify complete
Program succeeded.
```

> mdb 終了時に Java の `ChronicleHashClosedException` が出ることがありますが、
> その前に `Program succeeded.` が出ていれば書き込みは完了しています（無害）。

**reset** が成功すると次が出ます。warm/cold の判定や復旧が必要だった場合は `--verbose` 出力に詳細が出ます:

```text
PKOB4 Boost reset succeeded.
```

終了コード:

```text
0  成功
1  引数エラー（device 名の形違い等。提案が表示される）
2  MPLAB X / Java / IPECMDBoost が見つからない
3  reset 失敗
4  リトライ後もタイムアウト
5  予期しない例外
6  PKOB4 が wedge 状態（USB 抜き差しが必要・リトライ不可）
```

`flash_pkob4 --reset-after-flash` では、flash 自体は成功したが後続 reset が失敗した場合に exit code 6 を返します。

---

## うまくいかない時

- **MPLAB X IDE / IPE を開いていると失敗する**：IDE が PKOB4 を掴むため。IDE を閉じてから実行。
- **reset がタイムアウトする / 「Wait for current operation to complete」**：
  以前の boost サーバ（MPLAB 同梱 java）や stale な `2012.ini`/`2012.lock` が残っているのが主因。
  **`reset_pkob4` は自動で後始末します**（cold 起動前の stale file 削除、失敗時の `/OQ`、
  port-owner java kill、`2012.ini`/`2012.lock` 削除、cold retry）。
  それでも詰まる時の手動掃除:
  ```powershell
  Get-CimInstance Win32_Process -Filter "Name='java.exe'" |
    Where-Object { $_.ExecutablePath -like '*MPLABX*' } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Remove-Item "$env:USERPROFILE\.mchp_ipe\2012.ini" -Force -ErrorAction SilentlyContinue
  ```
- **exit 6 / `unloaded while still busy` / `unplug and reconnect`**：PKOB4 ファーム側が
  wedge した状態で、ホスト側の掃除では戻りません。**USB ケーブルを抜き差し**してから再実行。
  （`reset_pkob4` はこれを検知し、リトライで沼らず exit 6 で案内します。）
- **2 枚以上接続時に間違ったボードを操作してしまう**：`--serial` の付け忘れ。必ず付ける。
  分からなければ `--list` で確認。

---

## 注記

- exe はビルド成果物（各 約 68 MB）です。ソースリポジトリ `sulaolab/pkob4-flash-reset` には
  **含めず**（`.gitignore` 済み）、ソースのみ管理。このフォルダの exe は配置用です。
- ソースを直したら `tools/flash_pkob4` / `tools/reset_pkob4` / `tools/read_udid_pkob4` で
  `dotnet publish -c Release` し、生成された `bin\Release\net8.0\win-x64\publish\*.exe` を
  このフォルダへコピーして更新します。
- `legacy/` には旧 PowerShell スクリプト（`pkob4_java_full_clear.ps1` など）を残しています。
  通常は上記 3 つの exe だけで足ります。
