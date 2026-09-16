<img src="icon.png" width="64" align="left" alt="">

# Haiku 用 Prince of Persia

[English](README.md) | [한국어](README.ko.md)

Prince of Persia (1990) を Haiku のネイティブアプリとして動かします。下に DOS
エミュレータも SDL もありません。ゲームは普通の Haiku プロセスとして動きます。
画面は BWindow、音は BSoundPlayer、入力は Haiku のキーコードで処理します。

![Haiku で動くタイトル画面](screenshots/title.png)

ゲームのロジックは [SDLPoP](https://github.com/NagyD/SDLPoP) から持ってきました。
SDLPoP は DOS 版を逆アセンブルして C で再構成したプロジェクトで、本来は SDL2 の
上で動きます。この移植では SDLPoP が呼ぶ SDL の関数を、Be API で書いた小さな
プラットフォーム層 (`src/haiku`) がすべて処理します。ゲームデータは DOS 版 1.4
です。`data/` の `.DAT` ファイルにグラフィック、レベル、効果音、音楽が入って
います。DOS 用の `PRINCE.EXE` 自体は実行しません。

2026-09-16 に Haiku x86_gcc2 (hrev99002) で確認した内容:

- タイトル、オープニングのカットシーン、デモが再生されます。
- レベル 1 が始まり、王子を操作できます。
- 王子が画面の端を越えると、次の部屋に画面が切り替わります。
- タイトル音楽、足音、効果音が鳴ります。
- Alt+Enter で全画面になり、Ctrl+Q で終了します。
- デスクトップと Deskbar から起動できます。

![レベル 1](screenshots/level1.png)

## ビルドとインストール

ゲームデータはこのリポジトリに入っていません (`data/` は `.gitignore` にあります)。
まず手持ちの DOS 版 Prince of Persia 1.4 の `.DAT` ファイルを `data/` にコピー
してください。

```sh
mkdir -p data
cp /path/to/PRINCE/*.DAT data/
```

そのあと Haiku で:

```sh
./install.sh              # ビルドして ~/config/non-packaged/apps へインストール
./install.sh --build-only # ビルドのみ: build/PrinceOfPersia
./install.sh --uninstall  # アンインストール (セーブと設定は残します)
```

インストールするとデスクトップと Deskbar > Applications に「Prince of Persia」の
リンクができます。32 ビットのハイブリッドではセカンダリコンパイラでビルドします
(`setarch x86 make`)。必要なのはシステムライブラリの `libbe`、`libmedia`、
`libtranslation` だけです。

インストールせずにソースフォルダから動かすには `build/PrinceOfPersia` を実行して
ください。プログラムは自分の隣か一つ上の階層から `data/` を探します。

## 操作

DOS 版のキーはそのまま使えます。

| キー | 動作 |
| --- | --- |
| 矢印キー | 移動、ジャンプ、しゃがむ |
| Shift | ぶら下がる、拾う、斬る、慎重に歩く |
| Esc | 一時停止 |
| Space | 残り時間の表示 |
| Ctrl+A | レベルをやり直す |
| Ctrl+G / Ctrl+L | セーブ / ロード |
| Ctrl+S | 音のオンとオフ |
| Ctrl+R | タイトルへ戻る |
| Ctrl+Q | 終了 |

SDLPoP で追加されたキーもあります。

| キー | 動作 |
| --- | --- |
| Backspace | 設定メニュー |
| F6 / F9 | クイックセーブ / クイックロード |
| Alt+Enter | 全画面の切り替え |
| F12 | スクリーンショット、`screenshots/` に保存 |

## 設定

`SDLPoP.ini` は DOS 版と同じ挙動になるように設定してあります。

- SDLPoP の案内画面を出しません。
- Esc はメニューを開かず一時停止だけします。
- SDLPoP のゲームプレイのバグ修正を無効にしています。
- コピープロテクトの薬レベルを飛ばします。元フォルダのクラック済み 1.4 も飛ばします。

インストール後、設定ファイルはセーブデータと一緒に
`~/config/non-packaged/apps/Prince of Persia/` に置かれます。ゲーム内の設定メニュー
で変えた値は同じフォルダの `SDLPoP.cfg` に保存されます。

## 構成

| パス | 役割 |
| --- | --- |
| `src/engine/` | SDLPoP のゲームコード (GPLv3)。`config.h` のウィンドウタイトルだけ変更 |
| `src/haiku/SDL2/SDL.h`, `SDL_image.h` | エンジンが呼ぶ SDL2 API の部分を Haiku 層向けに宣言 |
| `src/haiku/platform.cpp` | BApplication と BWindow、フレーム表示、キーボードとマウス、タイマー、全画面 |
| `src/haiku/surface.cpp` | ソフトウェアサーフェス: パレット、カラーキー、ブレンド、ブリット、フォーマット変換 |
| `src/haiku/audio.cpp` | エンジンのミキサー (デジタル効果音と OPL 音楽合成) を BSoundPlayer で出力 |
| `src/haiku/image.cpp` | Translation Kit による PNG の読み書き |
| `src/haiku/rwops.cpp` | セーブ、設定、リプレイ用のファイルとメモリのストリーム |
| `data/` | DOS 版 1.4 のゲームデータ |
| `tools/make_icon.py` | ベクターアイコンと `resources/PrinceOfPersia.rdef` を生成 |
| `tools/rendericon.cpp` | HVIF アイコンを PNG に描画して確認 |
| `tools/sendkey.cpp` | 実行中のゲームにキーメッセージを送る SSH テスト用ツール |
| `tools/run-remote.sh` | SSH で Haiku 機にビルド、実行してログとスクリーンショットを回収 |

## 制限

- ジョイスティックとゲームパッドには対応していません。キーボードとマウスだけです。
- エンジンが再現するのは DOS 1.0 版のロジックで、読み込むデータは 1.4 版です。1.0 と 1.4 で挙動が違う部分は 1.0 に従います。

## ライセンス

エンジンと Haiku 層は GPLv3 です (`COPYING`)。`data/` のファイルは原作ゲームの
著作物です。手持ちのゲームから持ってきたものなので `data/` はバージョン管理から
外してあり、このコードと一緒に再配布してはいけません。

## AI 利用の開示

このプログラムは Claude と一緒に作りました。
