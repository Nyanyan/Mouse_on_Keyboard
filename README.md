# Mouse_on_Keyboard

キーボードに取り付けたマウスセンサーで PC のマウス操作をするための、自作ポインティングデバイスのファームウェアです。

USB マウスのセンサーを USB Host Shield 経由で Pro Micro (ATmega32u4) が読み取り、加速などの処理をしてから、PC には普通の USB マウスとして送ります。

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
| `SLOW_RATIO` | ゆっくり動かしたときの倍率 (`POINTER_SPEED` に対する割合) | 0.25 |
| `SPEED_LOW` / `SPEED_HIGH` | 加速が始まる速さ / 最大になる速さ (カウント/秒) | 250 / 1250 |
| `WHEEL_SPEED` | スクロールの速さ (1 カウントあたりのホイールのノッチ数) | 0.15 |

ポインタの倍率は、速さに応じて `POINTER_SPEED × SLOW_RATIO` から `POINTER_SPEED` まで直線的に変わります。

## 仕組み

1. USB Host Shield (`HIDUniversal`) がセンサーのレポートを受け取り、`buf[1]`, `buf[2]` の移動量を足し込んでいきます。
2. `SEND_INTERVAL_MS` ごとに、たまった移動量をまとめて取り出します。
3. 移動の速さ (縦横を合わせたベクトルの大きさ) から倍率を決めます。縦横で同じ倍率をかけるので、斜めに動かしても方向がずれません。
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

## クレジット

USB Host Shield 2.0 のサンプル [USBHIDJoystick](https://github.com/felis/USB_Host_Shield_2.0/tree/master/examples/HID/USBHIDJoystick) をもとに、Nyanyan が改変したものです。
