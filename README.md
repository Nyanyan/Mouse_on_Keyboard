# Mouse_on_Keyboard

キーボードに取り付けたマウスセンサーで PC のマウス操作をするための、自作ポインティングデバイスのファームウェアです。

USB マウスのセンサーを USB Host Shield 経由で Pro Micro (ATmega32u4) が読み取り、加速などの処理をしてから、PC には普通の USB マウスとして送ります。

## 書き込み時の注意

自分用メモ

ボードと周波数の設定を間違えなければ、分解せずに書き込めるので、とりあえず設定をちゃんとチェックして書き込んでみると良い。LilyPad Arduino USBとして書き込めばそれだけで動くはず。多分。

ボード設定をミスったら、分解してSparkFun Pro Micro (8MHz)のRSTピンとGNDピン(隣り合ってて便利)を2回素早くショートさせて、その隙に書き込む必要がある。これは大変なので、その時のCOMをAIに教えて、COMを発見した瞬間に自動で書き込むスクリプトを書いてもらうのが良い。

## 機能

- ポインタ移動 (速度に応じた加速つき)
- 左クリック・右クリック
- 中ボタンを押している間はスクロール (ポインタを縦に動かす操作をホイールに変換)
- Chrome Remote Desktop などのリモートデスクトップでも遅延しにくい送信方式

## ファイル構成

| パス | 内容 |
|---|---|
| [keyboard_mouse/keyboard_mouse.ino](keyboard_mouse/keyboard_mouse.ino) | 本体のスケッチ |
| [usb_host_shield/usb_host_shield.ino](usb_host_shield/usb_host_shield.ino) | 調査用スケッチ。USB Host Shield につないだ機器の HID レポートをそのままシリアルに表示します。センサーのどのバイトが X / Y なのかを調べるときに使います |
| [tools/measure_direction.ps1](tools/measure_direction.ps1) | 方向ごとの偏りの測定ツール (PC 側、PowerShell)。`SCALE_*` の推奨値を出します ([使い方](#方向ごとの偏りの測定)) |

## ハードウェア

- **Pro Micro 3.3V / 8MHz** (ATmega32u4。基板の水晶が `8.000` のもの)
- **USB Host Shield** (MAX3421E)
- **USB マウス** (センサーとして使用。90° 回転させて取り付けています)
- タクトスイッチ 3 個 (左・右・中ボタン)

### 配線

| 用途 | Pro Micro のピン |
|---|---|
| 左ボタン | D2 |
| 右ボタン | D3 |
| 中ボタン (押している間スクロール) | D4 |
| USB Host Shield SS | D10 |
| USB Host Shield INT | D9 |
| USB Host Shield SCK / MISO / MOSI | D15 / D14 / D16 |

ボタンは内部プルアップを使っているので、スイッチの片側をピンに、もう片側を GND につなぎます (押すと LOW)。

## 必要なソフトウェア

- Arduino IDE
- **USB Host Shield Library 2.0** (ライブラリマネージャからインストール)
- Mouse ライブラリ (Arduino IDE に付属)

## 書き込み方法

1. Arduino IDE でボードを **「LilyPad Arduino USB」** にします (ATmega32u4 / 8MHz の組み込み定義)。
   - SparkFun のボードパッケージを入れている場合は「SparkFun Pro Micro」→「ATmega32U4 (3.3V, 8 MHz)」でも OK です。
   - **Arduino Leonardo / Micro は 16MHz 用なので選ばないでください。** 8MHz の基板に書き込むと USB が認識されなくなります (ブートローダーは無事なので、下の手順で書き直せば戻ります)。
2. ポートを選んで、普通に書き込みます。

### 書き込めないとき

スケッチが USB として認識されていない場合 (ポートが出てこない場合) は、手動でブートローダーを起動します。

1. IDE で書き込みボタンを押します。
2. コンパイルが終わって「書き込み中…」になったら、**RST ピンを GND に素早く 2 回つなぎます** (リセットボタンなら素早く 2 回押す)。

リセット 1 回だとブートローダーは約 0.75 秒で終わってしまいますが、素早く 2 回リセットすると約 8 秒待ってくれます。

## 設定

[keyboard_mouse.ino](keyboard_mouse/keyboard_mouse.ino) の先頭の `#define` で調整できます。

| 定数 | 意味 | 初期値 |
|---|---|---|
| `LEFT_BUTTON` / `RIGHT_BUTTON` / `MIDDLE_BUTTON` | ボタンのピン | 2 / 3 / 4 |
| `DEBOUNCE_MS` | チャタリング対策。ボタンが変化した後、この時間は次の変化を無視する (ms) | 10 |
| `SWAP_XY` / `INVERT_X` / `INVERT_Y` | センサーの向きに合わせた軸の入れ替え・反転 | 1 / 1 / 0 |
| `SEND_INTERVAL_MS` | PC に送る間隔の最小値 (ms)。小さくすると滑らかになりますが、リモートデスクトップで遅延しやすくなります | 8 |
| `POINTER_SPEED` | 速く動かしたときの倍率 (センサー 1 カウントあたりの画面ピクセル数) | 4.0 |
| `SLOW_RATIO` | ゆっくり動かしたときの倍率 (`POINTER_SPEED` に対する割合) | 0.4 |
| `SPEED_LOW` / `SPEED_HIGH` | 加速が始まる速さ / 最大になる速さ (カウント/秒) | 250 / 1250 |
| `SCALE_LEFT` / `SCALE_RIGHT` / `SCALE_UP` / `SCALE_DOWN` | 方向ごとの補正倍率 (画面上の方向)。加速の前にかかります。このセンサーは同じ指の動きでも左方向を右方向の約 2.2 倍多く数えるので、左を小さく・右を大きくしています。値は[測定ツール](#方向ごとの偏りの測定)で決められます | 0.67 / 1.5 / 1.0 / 1.0 |
| `WHEEL_SPEED` | スクロールの速さ (1 カウントあたりのホイールのノッチ数) | 0.15 |

ポインタの倍率は、速さに応じて `POINTER_SPEED × SLOW_RATIO` から `POINTER_SPEED` まで直線的に変わります。

## 方向ごとの偏りの測定

センサーが方向によって違うカウント数を出す (例: 左だけ速い・右だけ遅い) ときに、[tools/measure_direction.ps1](tools/measure_direction.ps1) で偏りを測って `SCALE_*` の値を決められます。

### 使い方

1. [keyboard_mouse.ino](keyboard_mouse/keyboard_mouse.ino) を書き込んでおきます。測定用の出力は最初から入っているので、測定用に書き換える必要はありません。
2. Arduino IDE のシリアルモニタなど、同じポートを開いているものがあれば閉じます。
3. リポジトリのフォルダで次のコマンドを実行します (`COM9` は Arduino のポートに合わせてください)。

   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\measure_direction.ps1 -Port COM9
   ```

4. 画面の指示に従ってセンサーを動かします。
   - 指を**同じ 2 点の間で**、左右に 20 往復ほど動かします (最後は最初の位置に戻します)。画面上のポインタの位置ではなく、指の位置を揃えるのがポイントです。
   - 少し止めてから、上下も同じように 20 往復ほど動かします。
   - 測定中もマウスは普通に動きます。
5. 終わったら Enter キーを押します。方向ごとの集計と推奨値が表示されます。

   ```
   [左右]
     左: 合計   2392   往復  21 回   1 回あたり  104.0
     右: 合計   1067   往復  21 回   1 回あたり   44.0
     → 左は右の 2.24 倍
     推奨値:
       #define SCALE_LEFT 0.67
       #define SCALE_RIGHT 1.50
   ```

6. 推奨値を [keyboard_mouse.ino](keyboard_mouse/keyboard_mouse.ino) の `SCALE_*` に書き写して、書き込み直します。推奨値は補正前のカウント数から計算しているので、今の `SCALE_*` の値に関係なく、そのまま書き換えて使えます。

### オプション

| オプション | 意味 |
|---|---|
| `-Port COM9` | Arduino のシリアルポート |
| `-Seconds 120` | 測定の最長時間 (秒)。この時間が過ぎても終了します。初期値は 120 |
| `-OutCsv measure.csv` | 測定データを CSV に保存します |
| `-InputCsv measure.csv` | 保存した CSV を集計します (Arduino は不要) |

### 仕組み

- [keyboard_mouse.ino](keyboard_mouse/keyboard_mouse.ino) は、PC がシリアルポートを開いている (DTR が ON の) 間だけ、50ms ごとに `D,<ms>,<右>,<左>,<下>,<上>,<レポート数>` を出力します。値は軸の入れ替え・反転の後、`SCALE_*` をかける前のカウント数です。PC が読み取っていないときは出力を飛ばすので、マウスの動作が止まることはありません。
- 指を同じ 2 点の間で往復させれば、実際に動いた距離は行きと帰りで同じなので、カウント数の比がそのままセンサーの偏りになります。指を動かす速さにも左右されません。
- 推奨値は、比 r に対して √r で両側を補正します (例: 左が右の r 倍なら `SCALE_LEFT = 1/√r`, `SCALE_RIGHT = √r`)。左右の平均の速さは変わりません。

## 仕組み

1. USB Host Shield (`HIDUniversal`) がセンサーのレポートを受け取り、`buf[1]`, `buf[2]` の移動量を足し込んでいきます。
2. `SEND_INTERVAL_MS` ごとに、たまった移動量をまとめて取り出します。
3. 方向ごとの補正 (`SCALE_*`) をかけてから、移動の速さ (縦横を合わせたベクトルの大きさ) で倍率を決めます。縦横で同じ倍率をかけるので、斜めに動かしても方向がずれません。
4. 1 ピクセル未満の端数は次回に持ち越すので、ゆっくりした動きも失われません。
5. 動きがあるときだけ PC にレポートを送ります。1 回で送れない大きな動き (127 を超える分) は、その場で複数のレポートに分けて送ります。
6. ボタンを押したり離したりしたときは、それまでの移動を先に送ってからボタンの状態を送るので、クリック位置がずれません。

## トラブルシューティング

| 症状 | 原因と対処 |
|---|---|
| コンパイル時に `'Mouse' was not declared` / `This sketch needs a board with native USB` | USB 機能のないボード (Arduino Pro / Pro Mini など) が選ばれています。「LilyPad Arduino USB」を選んでください |
| ポインタが動かない (ボタンは動く) | センサーを読み取れていません。センサーを変えた場合は、[usb_host_shield.ino](usb_host_shield/usb_host_shield.ino) を書き込んでシリアルモニタ (115200bps) でレポートを確認し、移動量が入っているバイトに合わせて `SensorReportParser::Parse()` を直してください。なお、このセンサーは `HIDBoot` (boot protocol) では認識できませんでした |
| ポインタの向きがおかしい | `SWAP_XY` / `INVERT_X` / `INVERT_Y` を変えてください |
| 速すぎる / 遅すぎる | `POINTER_SPEED` と `SLOW_RATIO` を調整してください |
| 特定の方向だけ速い / 遅い | [測定ツール](#方向ごとの偏りの測定)で偏りを測り、推奨値を `SCALE_LEFT` / `SCALE_RIGHT` / `SCALE_UP` / `SCALE_DOWN` に設定してください |
| 測定ツールで「データがありません」と出る | センサーを動かしたか、測定用の出力が入った keyboard_mouse.ino が書き込まれているかを確認してください |
| 測定ツールで「を開けませんでした」と出る | ポート名と、Arduino IDE のシリアルモニタなどが同じポートを開いていないかを確認してください |

## クレジット

USB Host Shield 2.0 のサンプル [USBHIDJoystick](https://github.com/felis/USB_Host_Shield_2.0/tree/master/examples/HID/USBHIDJoystick) をもとに、Nyanyan が改変したものです。
