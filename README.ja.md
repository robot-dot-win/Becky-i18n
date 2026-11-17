[English](README.md) ｜ [简体中文](README.zh-CN.md) ｜ [日本語](README.ja.md)

<div align="center">

# ✉️ Becky-i18n

**Becky! Internet Mail 向け非侵襲型・多言語化（i18n）＆ローカライズプラグイン**

[![Release: Pre-release](https://img.shields.io/badge/Release-Pre--release-orange.svg)](https://github.com/robot-dot-win/Becky-i18n/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-806000.svg?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Win32%20(x86)-404040.svg?style=flat-square)](https://www.rimarts.jp/becky.htm)
[![C++20](https://img.shields.io/badge/std-C%2B%2B20-2b5b84.svg?style=flat-square)](https://en.cppreference.com/w/cpp/20)
[![Sponsor](https://img.shields.io/badge/Sponsor-555555?style=flat-square&logo=githubsponsors)](https://github.com/sponsors/robot-dot-win)
[![Patron](https://img.shields.io/badge/❤%EF%B8%8F-Patron-8b2635?style=flat-square)](patron/PATRON.ja.md)

</div>

**Becky-i18n**（`locale.dll`）は、**Becky! Internet Mail** のために設計された、非侵襲型かつデータ駆動型の多言語化（i18n）・ローカライズプラグインです。C++20 で記述され、Becky! 公式 Plug-In SDK、Win32 ウィンドウのサブクラス化、および最小限のスコープに絞った API フック（Detour）を組み合わせることで、**実行ファイル（`B2.exe`）を一切書き換えることなく**、外部の UTF-8 JSON 言語パックを用いて実行時に Becky! のユーザーインターフェースを任意の言語へと翻訳します。

---

## 概要（About）

### ホストアプリケーション：Becky! Internet Mail

[**Becky! Internet Mail**](https://www.rimarts.jp/becky.htm)（**有限会社リムアーツ / RimArts, Inc.** 開発）は、20 年以上にわたり進化を続ける Windows 向けの伝説的なメールクライアントです。Visual C++ による純粋な Win32 ネイティブアプリケーションとして構築されており、その優れた特性から世界中のパワーユーザーに愛され続けています。

* **長年の実績と継続的なメンテナンス**：20 年以上にわたる高い信頼性を誇り、最新の Windows 環境でも快適に動作します。
* **純粋な Windows ネイティブ実装**：肥大化した Web フレームワークに頼らず、Win32/MFC 上で直接構築されています。
* **極めて少ないリソース消費と圧倒的な高速性**：瞬時に起動し、現代のメールクライアントのわずか数分の一のメモリ消費で、数十万通のメールも軽快に処理します。
* **抜群の安定性**：クラッシュはほぼ発生しません。
* **クラシックで洗練された直感的 UI**：作業を妨げないシンプルな画面構成と、キーボード操作に最適化された設計を備えています。

### なぜ Becky-i18n なのか？

Becky! は内部的に **ANSI（MBCS）エンコーディング**を使用しており、UI 文字列は実行ファイル内にハードコードされています。また、公式に提供されているのは**英語版**と**日本語版**の 2 種類のみです。そのため、これまでインターネット上で流通していた他言語版（中国語、フランス語、ドイツ語など）は、`B2.exe` を直接バイナリ改造（パッチ適用）して作られており、デジタル署名の破損や公式アップデートへの追従遅れ、安定性の懸念といった課題を抱えていました。

**Becky-i18n** は、この問題を根本から解決します：

1. **バイナリ改変ゼロ**：公式 Plug-In SDK を通じて読み込まれるインプロセスプラグイン（`locale.dll`）として動作するため、常に無改変の公式 `B2.exe`（英語版ベース）をそのまま利用できます。
2. **100% データ駆動型**：新しい言語の追加や翻訳テキストの調整は、人間が読み書きしやすい UTF-8 `.json` ファイルを編集するだけで完結し、再コンパイルは不要です。
3. **高度な UI 翻訳カバレッジ**：標準的なメニューやコントロールだけでなく、複雑なオーナー描画メニュー（`ETO_GLYPH_INDEX`）、ダブルバッファリング描画されるカスタム MIME タブバー（`AfxWnd`）、動的なステータスバー領域、ツールバーのツールチップまで幅広く対応しています。

---

## 開発状況とロードマップ（`pre-release1`）

本リポジトリは現在 **`pre-release1`** 段階にあります。コア翻訳エンジン、メモリ安全機構、メインウィンドウ、メニューシステム、UI 言語選択ダイアログ、および 14 言語の言語パックが実装・検証済みです。

### `pre-release1` で完了した項目

* ✅ **メインウィンドウ（Main Window）**：
  * システムフォルダツリー（未読数 `(n)` の保持に対応）および動的ルートノード（「検索結果」など）。
  * メール一覧リストビューのカラムヘッダ（ListView Column Headers）。
  * メール表示ペインのヘッダラベルおよびタスク（Agent/Task）操作コントロール。
  * メール表示ペイン下部のオーナー描画 MIME タブバー。
  * ステータスバーの各動的ペイン。
* ✅ **メニューシステム ＆ UI 言語選択ダイアログ**：
  * メインウィンドウ、メール作成ウィンドウ（Compose）、住所録（Address Book）におけるトップレベルおよび階層ポップアップメニュー。
  * 固定コマンド、複数バリアントを持つコマンド、動的 ID 範囲、ツールバーのツールチップ、ステータスバーのヘルプヒントを統合したグローバルコマンドプール。
  * メールヘッダ項目のオーナー描画ポップアップメニュー。
  * モーダル形式の **UI 言語選択ダイアログ（UI Language Picker Dialog）**。
* ✅ **14 種類の国際言語データファイル（`locale/*.json`）**：
  * `en_US.json` — 英語（English / ベースラインテンプレート）
  * `de_DE.json` — ドイツ語（Deutsch）
  * `es_ES.json` — スペイン語（Español）
  * `fr_FR.json` — フランス語（Français）
  * `ja_JP.json` — 日本語
  * `ko_KR.json` — 韓国語（한국어）
  * `ms_MY.json` — マレー語（Bahasa Melayu）
  * `pt_PT.json` — ポルトガル語（Português）
  * `ru_RU.json` — ロシア語（Русский）
  * `th_TH.json` — タイ語（ไทย）
  * `uk_UA.json` — ウクライナ語（Українська）
  * `vi_VN.json` — ベトナム語（Tiếng Việt）
  * `zh_CN.json` — 中国語簡体字（简体中文）
  * `zh_TW.json` — 中国語繁体字（繁體中文）

### 今後の予定（To-Do）

* [ ] **メール作成ウィンドウ（`Compose` Window）**：子コントロールおよび関連ダイアログの完全ローカライズ。
* [ ] **住所録ウィンドウ（`Address Book` Window）**：子コントロール、プロパティシート、LDAP/CardDAV ビューの完全ローカライズ。
* [ ] **全般的な設定ダイアログ（`General Setup` Dialog）**：全設定タブおよびコントロールのローカライズ。
* [ ] **メールボックス設定ダイアログ（`Mailbox Setup` Dialog）**：アカウント、POP3/IMAP4/SMTP、プロファイル設定ダイアログのローカライズ。
* [ ] **その他のダイアログと UI 要素**：振り分けマネージャ（Filtering Manager）、メーリングリストマネージャ、メール検索ダイアログ、テンプレートエディタ、各種メッセージボックス。
* [ ] **ヘルプドキュメント（Help Documentation）**：ユーザーマニュアルおよびコンテキストヘルプのローカライズ。
* [ ] **さらなる言語データファイルの追加**：コミュニティによる対応言語の拡充と翻訳品質の向上。

---

## クイックスタート＆導入手順（一般ユーザー向け）

### 1. 前提条件：`Becky! Internet Mail`（英語版）のインストール

本プラグインは英語版の UI 文字列をベースラインとしてマッチングを行います。Becky! Internet Mail の**英語版（English edition）**を RimArts 公式サイトよりダウンロードしてインストールしてください。

* **公式サイト ダウンロードページ**：<https://www.rimarts.jp/becky.htm#download>

### 2. `Becky-i18n` の配置

1. [GitHub Releases](https://github.com/robot-dot-win/Becky-i18n/releases) ページより、最新の `Becky-i18n` リリースアーカイブをダウンロードします。
2. Becky! の **`PlugIns`** フォルダを開きます。Becky! の仕様により、以下のいずれかの場所にあります：
   * `[Becky! プログラムフォルダ]\PlugIns\`（例：`C:\Program Files (x86)\RimArts\B2\PlugIns\`）、**または**
   * `[Becky! データフォルダ]\PlugIns\`
3. `locale.dll` と `locale\` サブフォルダ（`.json` 言語ファイル群が含まれるフォルダ）を `PlugIns\` フォルダ内にコピーし、以下のような構成にします：

```text
PlugIns\
├── locale.dll          # Becky-i18n プラグイン本体（32-bit Win32, 静的 CRT リンク）
└── locale\             # 言語パック格納フォルダ
    ├── en_US.json      # 英語（デフォルト / ベースライン）
    ├── de_DE.json      # ドイツ語 (Deutsch)
    ├── es_ES.json      # スペイン語 (Español)
    ├── fr_FR.json      # フランス語 (Français)
    ├── ja_JP.json      # 日本語
    ├── ko_KR.json      # 韓国語 (한국어)
    ├── ms_MY.json      # マレー語 (Bahasa Melayu)
    ├── pt_PT.json      # ポルトガル語 (Português)
    ├── ru_RU.json      # ロシア語 (Русский)
    ├── th_TH.json      # タイ語 (ไทย)
    ├── uk_UA.json      # ウクライナ語 (Українська)
    ├── vi_VN.json      # ベトナム語 (Tiếng Việt)
    ├── zh_CN.json      # 中国語簡体字 (简体中文)
    └── zh_TW.json      # 中国語繁体字 (繁體中文)
```

> **補足**：`locale.dll` を配置した後に初めて Becky! を起動すると、新しいプラグイン `locale.dll` を読み込むか確認するダイアログが表示される場合があります。その際は **Yes（はい）** をクリックしてください。

### 3. 表示言語の切り替え

1. メニューバーの **Tools → UI Language**（または翻訳後の **ツール → 表示言語**）をクリックし、希望する言語を選択します。
2. **Becky! を再起動**すると、選択した言語が反映されます。
   * *技術的補足*：**OK** をクリックすると、選択されたロケール識別子（例：`ja_JP` や `zh_CN`）が Windows レジストリの `HKEY_CURRENT_USER\Software\RimArts\B2\Settings` 内の `Locale` 値（`REG_SZ` 型、既定値は `en_US`）に保存されます。動作の安定性を最優先するため、セッション途中で既存のウィンドウハンドルを無理に書き換えるのではなく、起動時にクリーンに翻訳を適用する設計としています。

> **⚠️ Windows システムロケールに関する重要な注意（メニューやフォルダツリーが `"?"` と表示される場合）**：
> Becky!（`B2.exe`）は内部的に ANSI/MBCS アプリケーションであるため、一部の Win32/MFC 描画パスは Windows の **Unicode 対応ではないプログラムの言語**（システム ANSI コードページ）に依存します。言語を切り替えた後にメニューやフォルダツリーが疑問符（`"?"`）になってしまう場合は、以下の手順でシステムロケールを変更してください：
> 1. Windows の **コントロール パネル → 時計と地域 → 地域 → 管理** タブを開きます。
> 2. **システム ロケールの変更...** をクリックし、表示したい UI 言語に対応する地域・言語を選択します。
> 3. PC を再起動します。

---

## 言語パック仕様（翻訳作成者・コントリビューター向け）

Becky! に新しい言語を追加するのに**プログラミングの知識は不要**です。`PlugIns\locale\` フォルダ内の `en_US.json` をコピーして対象のロケール名（例：`it_IT.json`、`pl_PL.json`）にリネームし、`dst`、`replace`、`hint_dst` フィールドを翻訳するだけで作成できます。

### 1. ファイル名・文字コード・トップレベル構造

* **ファイル名**：`<locale_id>.json`。`<locale_id>` はセキュリティホワイトリスト正規表現 `^[A-Za-z0-9_-]{1,32}$` に一致する必要があります（例：`en_US`、`ja_JP`、`fr_FR`）。
* **文字コード**：必ず **BOM なし UTF-8（UTF-8 without BOM）** で保存してください。
* **トップレベルキー**：すべての言語 JSON ファイルは、以下の 3 つのトップレベルオブジェクトで構成されます：
  1. `"meta"`：メタデータおよびアクセスキー（ニーモニック `&`）の整形ルール。
  2. `"menus"`：トップレベルメニュー定義。`"popups"` と `"commands"` に分かれています。
  3. `"windows"`：Win32 ウィンドウクラス名（例：`"Becky2MainFrame"`）をキーとするウィンドウおよび子コントロール階層ツリー。

```json
{
  "meta": { ... },
  "menus": {
    "popups": { ... },
    "commands": [ ... ]
  },
  "windows": {
    "Becky2MainFrame": { ... }
  }
}
```

### 2. メタデータ（`meta`）とアクセスキー（`&`）の規約

```json
"meta": {
  "locale": "zh_CN",
  "name": "简体中文 (zh_CN)",
  "access_key": "$1(&2)$3",
  "version": "1.0.7",
  "author": "Martin Young"
}
```

| フィールド | 型 | 説明 |
| :--- | :--- | :--- |
| `locale` | `string` | ファイル名と一致する一意のロケール識別子（例：`"zh_CN"`、`"ja_JP"`）。 |
| `name` | `string` | **UI Language** ダイアログのドロップダウンリストおよび **About Becky! I18n** ダイアログに表示される言語名（例：`"日本語 (ja_JP)"`）。 |
| `access_key` | `string` | ニーモニック（`&`）の整形モード。`"as_is"` または `$1`、`&2`、`$3` を用いたパターンマッチングテンプレートを指定します。 |
| `version` | `string` | 言語ファイルのバージョン（例：`"1.0.7"`）。 |
| `author` | `string` | 言語パックの作成者・メンテナ名。 |

#### `src` 文字列の正規化ルール

言語ファイル内では、メニューおよびアクセスキーを持つコントロールの `src` 文字列はすべて**正規化**されています：

1. 末尾の三点リーダー（`...`）は `src` から除去されています（例：`"&Import..."` は `"Import"` として格納）。
2. 単独のニーモニック `&` や末尾の括弧付きアクセスキー（`(&Z)` など）は `src` から除去されています。
3. Win32 文字列でリテラルの `&` を表す二重アンパサンド（`&&`）は、単一の `"&"` に畳み込まれています。

#### 実行時における `dst` の処理（`as_is` モード vs. パターンマッチングモード）

`locale.dll` は `meta.access_key` の設定に応じて 2 種類のモードをサポートします：

##### モード A：`"as_is"` モード（`en_US.json` で使用）

* **動作**：ランタイムは `dst` を**一切加工せずそのまま出力**します。
* **作成者の責任**：アクセスキー文字の直前への `&` の配置、リテラル `&` の `&&` へのエスケープ、および末尾の `...` の付与まで、最終的な表示形式をすべて作成者自身が記述します（例：`"src": "Delete Folder"`, `"dst": "De&lete Folder..."`）。

##### Mode B：パターンマッチングモード（例：`"$1(&2)$3"`）

* **動作**：`dst` には**正規化されたプレーンテキストのみ**を記述します。作成者は `dst` にアクセスキー（`&`）や末尾の `...` を**手動で記述してはいけません**（例：`"src": "Delete Folder"`, `"dst": "フォルダ削除"`）。ランタイム側で「リテラル `&` のエスケープ＋アクセスキー挿入＋末尾 `...` の復元」を一括処理します。
* **パターン内のプレースホルダ定義**：
  * **`$1`**：メインのテキスト文字列（アクセスキーと末尾の三点リーダーを除いた `src` / `dst` 本文）。
  * **`&2`**：実行時の Win32 `src` 文字列から抽出された**アクセスキー**（例：`&F`）。
  * **`$3`**：**末尾の三点リーダー（`...`）**。実行時の Win32 `src` に元々 `...` が付いていた場合のみ `"..."` となり、付いていなければ空文字列 `""` になります。
* **よく使われるパターン記述例**（実行時の `src` = `"Save &As..."`、`dst` = `"另存为"` の場合）：

| パターン（`meta.access_key`） | 配置スタイル | `"Save &As..."` の描画結果 |
| :--- | :--- | :--- |
| **`"$1(&2)$3"`** | 末尾丸括弧スタイル（日中韓の標準的な表記） | `"另存为(&A)..."` |
| **`"[&2]$1$3"`** | 先頭角括弧スタイル | `"[&A]另存为..."` |
| **`"&2-$1$3"`** | 先頭ハイフンスタイル | `"&A-另存为..."` |
| **`"&2. $1$3"`** | 先頭ドットスタイル | `"&A. 另存为..."` |

### 3. メニューシステム（`menus`）

メニュー定義は個別のウィンドウから分離され、トップレベルの `"menus"` 直下に `"popups"` と `"commands"` として配置されます。

#### 3.1 `menus.popups`（ポップアップメニューツリー）

固定の `cmd_id` を持たないポップアップコンテナ（サブメニュー）は、`"main_frame"`、`"compose"`、`"address"` の 3 つのウィンドウバケットに分類されます。各ノードは `translate`、`src`、`dst`、および任意の階層的な `children` を持ちます。

#### 3.2 `menus.commands`（グローバルコマンドプール）

Becky! 内のすべての実行可能なメニュー項目は、`cmd_id` の昇順でフラットな配列に格納されます。Win32 のコマンド ID はアプリケーション全体で共通の意味を持つため、この単一のプールによって**メニュー項目**、**ツールバーボタンのツールチップ**、および**ステータスバーのヘルプテキスト（`hint_src` / `hint_dst`）**が同時に翻訳されます。

`menus.commands` は以下の 3 つのノード形式をサポートしています：

1. **固定単一コマンド（`cmd_id` + `src`/`dst`）**
2. **複数バリアントコマンド（`cmd_id` + `rules`）**：同じ `cmd_id` でもウィンドウによって異なるテキストが表示される場合に使用します（例：`57604` や `57636`）。
3. **動的コマンド ID 範囲（`cmd_id` + `cmd_id_end` + `rules`）**：Becky! が実行時に動的に ID を割り当てる範囲に使用します。`rules` に含まれないテキスト（ユーザーが作成したダイヤルアップ接続名など）は安全に原文のまま保持されます：
   * `1`–`100`（`"ownerdraw": true`）：メールヘッダ項目のオーナー描画ポップアップメニュー。
   * `10000`–`10099`：エディタ、デコード、段落整形、文字コード、およびツールバー専用コマンド。
   * `20000`–`31999`：プラグインコマンド範囲（PGP プラグイン、音声添付プラグイン、Becky-i18n プラグインなど）。
   * `55001`–`55099`：View → Language（文字コード）メニュー項目。
   * `56001`–`56099`：ダイヤルアップ接続メニュー項目（`(no dialup)`）。
   * `56100`–`56199`：Tools → Plug-Ins Setup 動的項目。

### 4. ウィンドウ＆コントロールツリー（`windows`）

`"windows"` 直下の各エントリはトップレベルウィンドウクラス（例：`"Becky2MainFrame"`）に対応し、`"children"` によって Win32 子コントロールの階層構造を再帰的に表現します。各コントロールは `class_name` と `ctrl_id` で照合されます。

| コントロールクラス（`class_name`） | `ctrl_id` | JSON 内の翻訳設定 |
| :--- | :--- | :--- |
| **`SysTreeView32`** | `1000` | システムフォルダツリー。`"image_indices": [12..23]` によってシステム既定フォルダを識別し、`"root_nodes"` で動的ルートフォルダを識別します。ユーザー作成フォルダは一切変更されません。 |
| **`SysHeader32`** | `0` | メール一覧 `SysListView32` 内のカラムヘッダ。 |
| **`AfxWnd`（タブバー）** | `12562` | メール表示ペイン下部のオーナー描画 MIME タブバー。 |
| **`Static`** | 各種 | 標準的なテキストラベル。 |
| **`Button`** | 各種 | 標準的なプッシュボタン。 |
| **`Edit`** | 各種 | 標準的なエディットボックス。 |
| **`ComboBox`** | `1532` | 0 始まりのインデックス（`index`）とテキストで照合されるドロップダウンリスト項目。 |
| **`msctls_statusbar32`** | `59393` | `index` によって複数の `"panes"` に分割された下部ステータスバー。 |

### 5. 翻訳マッチングルール

メニューノード、ウィンドウコントロール、ステータスバーペインにおける翻訳ルールは、以下の 2 つの直交する軸で構成されます：

1. **ルールの数（単一ルール vs. 複数ルール）**：
   * **単一ルールマッチング（Single-Rule）**：1 つのテキストパターンのみを照合する場合、マッチング用フィールドをその JSON ノードのプロパティとして直接記述します。
   * **複数ルールマッチング（Multi-Rule、`"`rules`"` 配列）**：1 つのノード（動的コマンド範囲、複数バリアントコマンド、タブバー、ステータスバーペインなど）で複数の候補文字列や正規表現パターンを照合する場合、`"rules": [ ... ]` 配列内に記述します。
2. **マッチングモード（完全一致マッピング vs. 正規表現）**：
   * **完全一致マッピング（`"src"` → `"dst"`）**：デフォルトモード。高速な文字列一致（または正規化コマンド検索）を行い、`"src"` を `"dst"` に置き換えます。
   * **正規表現モード（`"mode": "regex"` + `"pattern"` / `"replace"`）**：`"mode": "regex"` を指定することで有効化されます。ECMAScript 正規表現（`"pattern"`）を使用し、`"replace"` 内でキャプチャグループ置換（`$1`、`$2` など）を利用できます。

| ルール数 \ モード | 完全一致マッピング（`src` → `dst`） | 正規表現マッチング（`mode: "regex"` + `pattern` / `replace`） |
| :--- | :--- | :--- |
| **単一ルール** *（インライン属性）* | ノードに直接 `"src"` と `"dst"` を定義。 | ノードに直接 `"mode": "regex"`、`"pattern"`、`"replace"` を定義。 |
| **複数ルール** *（`"rules"` 配列）* | `"rules": [ { "src": "...", "dst": "..." }, ... ]` | `"mode": "regex"` と `"rules": [ { "pattern": "...", "replace": "..." }, ... ]` を併用。 |

#### 5.1 単一ルール ＋ 完全一致マッピング
単一の固定テキストラベルを持つコントロールや固定コマンドに使用します：

```json
{
  "class_name": "Button",
  "ctrl_id": 1068,
  "control_subtype": "PushButton",
  "translate": true,
  "src": "Edit Task",
  "dst": "编辑任务"
}
```

#### 5.2 複数ルール ＋ 完全一致マッピング
単一のコマンド ID、動的コマンド範囲、またはカスタムコントロールが実行時に複数の固定文字列のいずれかを表示する場合に使用します：

```json
{
  "cmd_id": 57636,
  "translate": true,
  "rules": [
    { "src": "Find", "dst": "查找" },
    { "src": "Search Address", "dst": "查找地址" },
    { "src": "Find Address", "dst": "查找地址" }
  ],
  "hint_src": "Find the specified text",
  "hint_dst": "查找指定文本"
}
```

#### 5.3 単一ルール ＋ 正規表現マッチング
コントロールやステータスバーペインが実行時変数（行・列番号やインライン宛先プレフィックスなど）を含む単一の動的フォーマットを表示する場合に使用します：

```json
{
  "index": 2,
  "translate": true,
  "mode": "regex",
  "pattern": "^R:(\\d+)(\\s+)C:(\\d+)(\\s+)Lines:(\\d+)(\\s*)$",
  "replace": "行:$1$2列:$3$4总行:$5$6"
}
```

#### 5.4 複数ルール ＋ 正規表現マッチング
1 つのコントロールやステータスバーペインが複数の異なる動的フォーマットを表示し、それぞれに正規表現キャプチャグループが必要な場合に使用します：

```json
{
  "index": 0,
  "translate": true,
  "mode": "regex",
  "rules": [
    {
      "pattern": "^Switch language to (.*)$",
      "replace": "切换界面语言为 $1"
    },
    {
      "pattern": "^Receiving message \\((\\d+)/(\\d+)\\)\\.\\.\\.$",
      "replace": "正在接收邮件 ($1/$2)..."
    }
  ]
}
```

### 6. 実行時の安全制限（セーフティリミット）

ホストプロセス（`B2.exe`）の安定性を保証するため、`locale.dll` は `.json` 言語パックの読み込み時に厳格な検証制限（`src/limits.hpp`）を適用します。いずれかの制限を超過した場合、プラグインはその不正なファイルやルールを拒否し、安全に標準の `en_US` UI へとフォールバックします：

| 制限項目定数 | 上限値 | 説明 |
| :--- | :--- | :--- |
| `kMaxJsonFileBytes` | `2 MB` | 単一の `.json` 言語ファイルの最大サイズ（UTF-8 バイト数）。 |
| `kMaxNestingDepth` | `20` | JSON オブジェクト／配列の最大ネスト深度。 |
| `kMaxStringBytes` | `4096` | 単一の JSON 文字列値の最大 UTF-8 バイト長。 |
| `kMaxRegexPatternBytes` | `256` | 正規表現 `pattern` の最大 UTF-8 バイト長。 |
| `kMaxRegexInputChars` | `1024` | 実行時に正規表現で評価される文字列の最大 UTF-16 文字数。 |
| `kMaxArrayElements` | `256` | 単一の JSON 配列（`commands`、`rules`、`children` など）に許可される最大要素数。 |
| `kMaxLocaleFiles` | `32` | `locale\` ディレクトリ内でスキャンされる `.json` ファイルの最大数。 |
| `kMaxLocaleIdLength` | `32` | ロケール識別子の最大文字長（`^[A-Za-z0-9_-]{1,32}$`）。 |
| `kMaxLogFileBytes` | `1 MB` | 診断ログファイルの最大サイズ（超過時は自動的に切り詰め・再作成）。 |

---

## 技術アーキテクチャとソースからのビルド（開発者向け）

### 1. コア翻訳エンジン

Becky! は UI 文字列をバイナリ内にハードコードしており、標準 Win32 コントロール、MFC ビュー、およびカスタム GDI オーナー描画サーフェスを組み合わせて使用しています。そのため、`locale.dll` では 2 つの経路を組み合わせたハイブリッドなフックアーキテクチャを採用しています：

1. **SDK コールバック ＋ ウィンドウサブクラス化（`UiTranslator`）**：
   * Becky! がエクスポートする `BKC_On*` ライフサイクルコールバックをフックします。
   * ウィンドウの HWND 階層を再帰的に走査し、対象ウィンドウプロシージャをサブクラス化して各種通知メッセージをインターセプトします。
2. **最小スコープの Win32/GDI API フック（MinHook による `ApiHooks`）**：
   * オーナー描画ヘッダポップアップメニューのフック（`ExtTextOutW` ＋ 拡張メタファイル `EMF` オフスクリーン再生による平文抽出）。
   * オーナー描画 MIME タブバーのフック（サブクラス化スコープ検知 ＋ ダブルバッファメモリ DC の `DrawTextA` フック）。
   * 非対象パスのゼロオーバーヘッド通過とメモリ安全性（`win32_guard.hpp`）：フックやコールバック内で参照されるすべてのポインタに対してユーザーモードアドレス範囲チェックと SEH メモリ読み取りプローブを実施し、例外が `B2.exe` へ漏出することを完全に防ぎます。

### 2. 開発環境とサードパーティ依存関係

#### コンパイラおよびプラットフォーム要件

* **OS**：Windows 10 以降。
* **コンパイラ**：**Visual Studio 2022** 以降（**C++ によるデスクトップ開発** ワークロード / MSVC ツールチェーン）。
* **C++ 標準規格**：**C++20**（`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_EXTENSIONS OFF`, `/utf-8 /W3 /permissive- /EHsc`）。
* **ターゲットアーキテクチャ**：**32-bit x86（`Win32`）専用**。Becky!（`B2.exe`）は 32 ビットアプリケーションであるため、CMake は `CMAKE_SIZEOF_VOID_P` を厳密にチェックし、64 ビット構成が指定された場合は `FATAL_ERROR` で停止します。
* **CRT リンク方式**：**静的 CRT リンク**（Release 時は `/MT`、Debug 時は `/MTd`）。VC++ 再頒布可能パッケージがインストールされていない環境でもそのまま動作します。
* **ビルドシステム**：**CMake ≥ 3.15**

#### 公式 SDK および外部ライブラリ（オフラインローカルビルド）

設計上、`CMakeLists.txt` はビルド時にインターネットからコードを取得しません。公式の **Becky! Plug-In SDK** と 2 つのオープンソースライブラリを事前にローカルへ配置する必要があります：

1. **Becky! 公式 Plug-In SDK（`Becky-SDK`）**
   * **ダウンロード**：Becky! 公式ダウンロードページより入手可能：<https://www.rimarts.jp/becky.htm#download>
   * **必要なファイル**：`BeckyAPI.cpp` および `BeckyApi.h`
2. **nlohmann/json**（Modern C++ 向けヘッダオンリー JSON ライブラリ）
   * **リポジトリ**：<https://github.com/nlohmann/json>
3. **MinHook**（Windows 向け軽量 x86/x64 API フックライブラリ）
   * **リポジトリ**：<https://github.com/TsudaKageyu/minhook>

#### 推奨ディレクトリ構成

`Becky-SDK`、`nlohmann/json`、`minhook` を本リポジトリ `Becky-i18n` と同じ親ディレクトリ直下に並べて配置します：

```text
<workspace>/
├── Becky-i18n/          # 本リポジトリ
│   ├── CMakeLists.txt   # トップレベル CMake ビルドスクリプト
│   ├── locale.def       # モジュール定義ファイル
│   └── src/             # C++ ソースファイル群
├── Becky-SDK/           # Becky! Plug-In SDK（BeckyAPI.cpp と BeckyApi.h を格納）
├── nlohmann/
│   └── json/            # git clone https://github.com/nlohmann/json.git
└── minhook/             # git clone https://github.com/TsudaKageyu/minhook.git
```

### 3. `locale.dll` のビルド手順

#### 方法 A：デフォルトの同階層ディレクトリ構成を使用する場合

PowerShell またはコマンドプロンプトを開き、以下を実行します：

```powershell
# 1. 依存ライブラリを同階層ディレクトリにクローン
git clone https://github.com/nlohmann/json.git nlohmann/json
git clone https://github.com/TsudaKageyu/minhook.git minhook
# （BeckyAPI.cpp と BeckyApi.h を含む Becky-SDK/ も同じ親フォルダに配置してください）

# 2. Becky-i18n の構成とビルド（必ず -A Win32 を指定）
cd Becky-i18n
cmake -S . -B build -A Win32
cmake --build build --config Release
```

#### 方法 B：依存ライブラリのパスを個別に指定する場合

SDK やサードパーティリポジトリを別の場所に配置している場合は、`-D` オプションで絶対パスを指定します：

```powershell
cd Becky-i18n
cmake -S . -B build -A Win32 `
  -DBECKY_SDK_DIR="D:/sdk/Becky-SDK" `
  -DNLOHMANN_JSON_DIR="D:/third_party/json" `
  -DMINHOOK_DIR="D:/third_party/minhook"

cmake --build build --config Release
```

ビルドが完了すると、`build/Release/locale.dll` にプラグインバイナリが出力されます。

---

## 知的財産権・謝辞・サードパーティ表記

本プロジェクトは、すべての原著作者およびサードパーティ製オープンソースプロジェクトの知的財産権を尊重しています：

1. **Becky! Internet Mail & Becky! Plug-In SDK**
   * **Copyright © RimArts, Inc. All rights reserved** (<https://www.rimarts.jp/>)
   * *Becky! Internet Mail* は有限会社リムアーツ（RimArts, Inc.）の登録商標およびプロプライエタリソフトウェアであり、乗松 知博（Tomohiro Norimatsu）氏によって開発されています。
   * `Becky-i18n` はコミュニティ主導で開発されている独立したオープンソースプラグインであり、有限会社リムアーツとの提携、協賛、または公式な承認関係はありません。
2. **JSON for Modern C++ (`nlohmann/json`)**
   * **Copyright © 2013–2026 Niels Lohmann** (<https://github.com/nlohmann/json>)
   * **MIT License** の下で提供されています。
3. **MinHook — The Minimalistic x86/x64 API Hooking Library for Windows**
   * **Copyright © 2009–2017 Tsuda Kageyu. All rights reserved** (<https://github.com/TsudaKageyu/minhook>)
   * **BSD 2-Clause License** の下で提供されています。
4. **Becky-i18n (`locale.dll` & Language Packs)**
   * **Copyright © 2026 Martin Young & Becky-i18n Contributors**
   * **MIT License** の下で提供されています。

---

## ご支援・スポンサーシップ（Patronage & Sponsorship）

🙏本プロジェクトへの温かいご支援が、さらなる開発を進める直接の原動力となります：[![Patron](https://img.shields.io/badge/❤%EF%B8%8F-Patron-8b2635?style=flat-square)](patron/PATRON.ja.md)

---

## ライセンス（License）

本プロジェクトは [MIT License](LICENSE) の下で公開されているオープンソースソフトウェアです。