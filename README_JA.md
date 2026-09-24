# ZMK Absolute-to-Relative Input Processor

[![Test](https://github.com/amgskobo/zmk-input-abs2rel/actions/workflows/test.yml/badge.svg)](https://github.com/amgskobo/zmk-input-abs2rel/actions/workflows/test.yml)

[English](README.md)

絶対座標のポインター入力を相対移動へ変換し、2 サンプルで平滑化する ZMK input
processor です。指が *どこにあるか* を報告するトラックパッドやタッチセンサーを、
*どれだけ動いたか* を必要とするポインター処理チェーンへ接続します。

## この module が独立している理由

これは置き換えではなく独自の processor です。ZMK には scaler、transform、
code-mapper、temp-layer などの input processor があり、
[zmk-input-processors](https://github.com/amgskobo/zmk-input-processors) は
そのうち 4 つを実行時に設定変更できるようにしたものです。どれも `runtime-*` と
名付けられており、chain を見ただけで実行中に変更できる stage が分かります。

この processor には upstream に対応するものがないため、prefix を付けても何も
表しません。以前は prefix のない名前のまま例外として文書化していましたが、
例外であること自体が問題の兆候でした。他の独自 processor
([vector-acceleration](https://github.com/amgskobo/zmk-input-vector-acceleration)、
[inertia](https://github.com/amgskobo/zmk-input-inertia)、
[padstick](https://github.com/amgskobo/zmk-input-padstick))と同じく、
専用の module に置いています。

## 導入

```yaml
manifest:
  remotes:
    - name: amgskobo
      url-base: https://github.com/amgskobo
  projects:
    - name: zmk-input-abs2rel
      remote: amgskobo
      revision: main
```

## 使用方法

```dts
#include <behaviors/input_processor_absolute_to_relative.dtsi>

&trackpad_listener {
    input-processors = <&zip_absolute_to_relative>;
};
```

別のボタン抑止方針が必要な場合や、名前が重要な場合(node 名は設定キーになります。
後述)は、node を自分で宣言します。

```dts
pointer_abs_rel: pointer_abs_rel {
    compatible = "zmk,input-processor-absolute-to-relative";
    #input-processor-cells = <0>;
};
```

`INPUT_BTN_TOUCH` の抑止は、手動で宣言した node を含めて既定で有効です。既存の
overlay や、方針を明示したい overlay がそのままビルドできるよう、property の
明示も引き続き受け付けます。

Devicetree の boolean property は存在することで `true` を表し、`false` の値を
持てません。そのため runtime custom settings を使わないビルドでは、`BTN_TOUCH` は
常に抑止されます。`false` へ切り替える必要がある場合は、後述の custom settings
option を有効にしてください。

### 標準 node

module は 2 つの標準 node を提供します。各 node は ZMK の input listener ごとに
独立した変換状態を持つため、ローカルの device と split 経由の device で同じ参照を
安全に共有できます。

| 参照 | node 名 | ボタン処理 |
| :--- | :--- | :--- |
| `zip_absolute_to_relative` | `abs_rel` | `BTN_TOUCH` を抑止し、`BTN_0` のクリックは通す |
| `zip_absolute_to_relative_scroll` | `abs_rel_scroll` | scroll 用に `BTN_TOUCH` と `BTN_0` の両方を抑止 |

状態は processor node と `input_device_index` の組に属します。ZMK は input
listener の instance index を渡すため、同じ node を使う 2 つの listener でも、
基準座標、平滑化履歴、抑止済みボタンの記録は分かれます。設定は node に属し、
それらの listener で共有されます。別のボタン方針や個別に調整できる設定項目が
必要な場合だけ、短い名前の instance を追加で宣言してください。

### 設定リファレンス

| Property | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `suppress-btn-touch` | bool | true | 基準点の破棄に使った後の `INPUT_BTN_TOUCH` を消費し、mouse HID に button 0 として届かないようにします。 |
| `suppress-btn0` | bool | false | トラックパッドが物理クリックとして報告する `INPUT_BTN_0` を消費します。 |

後述の設定 option を有効にすると、どちらも実行時の値になります。これらはパッドの
物理クリックをそもそも host へ届けるかを決めるもので、決め打ちするより試して
決めるべき値です。タップでクリックできるパッドでは、一方の設定ではボタンが重複し、
もう一方では欠けます。どちらになるかはパッドによって異なります。

実行時の設定で `suppress-btn-touch` を無効にすると、通過する touch edge は
それぞれ単独の sync 境界になります。edge の直後の座標は新しい基準点として
消費されるため、別の境界がないと release が送られないまま残り、host で mouse
button 0 が押されたままになるからです。

`suppress-btn0` を無効にしても、すでに抑止した press が host へ送られることは
ありません。その press の release は、レイヤー変更をまたぐ場合と同じ理由で
そのまま通します。`suppress-btn0` は、ここで press を抑止していない `BTN_0` の
release を決して破棄しません。release を通すことは常に安全です(対応する press は
すでに host に届いています)。一方、破棄するとそのボタンは押されたまま、解除する
手段がなくなります。この記録もレイヤー変更時にクリアされます。

## 動作

### 平滑化

移動量は、現在の差分と直前の差分の平均で平滑化します。

```text
smooth = (current_delta + previous_delta) / 2
```

半分にする処理は shift ではなく除算です。shift は負の無限大方向へ丸めるため、
同じ経路でも向きによって移動量が長く計測されてしまうからです。各軸の最初の
サンプルは基準点を作るだけでイベントを出さず、平滑化は 2 つ目から始まります。

### 基準点とレイヤー変更

基準点は input listener ごとにあります。`BTN_TOUCH` は press と release の両方で
その listener の基準点だけを破棄し、レイヤー変更はすべての listener の基準点を
無効にします。絶対座標イベントは、接触中かどうかを確認せず届くたびに変換します。
processor の経路は、接触のうちその経路が chain を担当している部分しか見ないため、
接触中かどうかを判断すると、別の経路で始まった接触の残りの間ずっと出力が
止まってしまうからです。

どの processor が動くかは、その時点で有効なレイヤーからイベントごとに決まるため、
1 つの接触が 2 つの chain に分かれることがあります。対策がないと、接触が移った先の
instance は以前のタッチで作った基準点を持ったままで、最初のサンプルを無関係な
2 つの接触の間の距離として扱います。実際には 1 count しか動いていないのに、
パッドを横切るほど跳びます。そのため `zmk_layer_state_changed` を直接購読し、
すべての基準点を無効にします。callback は共有の atomic generation を 1 つ進める
だけです。各 listener の stream は次にイベントを受け取ったときにその generation を
適用し、イベントの処理中に generation が変わった場合はそのイベントを破棄して
新しい状態から始めます。これにより、レイヤーの callback と座標変換が同じ状態へ
同時に書き込むことはありません。

### 値の範囲

変換 core は、Zephyr の input event が使う符号付き `int32_t` の全範囲を保持します。
その範囲を超える差分は、折り返さずに平滑化の前で飽和させます。出力は、下流の
processor と listener に向けた `int32_t` の相対イベントのままです。

## 実行時にフラグを変更する

`CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS=y` にすると、2 つのフラグが
[zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings)
を通じて `amgskobo__a2r` subsystem の下に公開され、Studio client が専用の画面なしで
一覧・編集できます。

キーは、所有する node の devicetree 名に field を続けたものです。

```
abs_rel.suppress_btn_touch
abs_rel_scroll.suppress_btn0
```

node 名は、問題の両側がすでに持っている唯一の識別子です。chain を描く view は、
各 listener の processor を devicetree からたどって stage ごとに
`const struct device *` を得ます。その `->name` は `DEVICE_DT_NAME()`、つまり
`DT_NODE_FULL_NAME()` であり、キーを作るのと同じ文字列です。したがって stage の
設定は、ちょうどその device 名で始まるキーになり、登録も module 間の取り決めも
devicetree への記述も必要ありません。

### 名前の長さ制限

最長の field は 18 文字の `suppress_btn_touch` です。保存される完全な名前から、
node 名に使えるのは **14 文字** です。48 byte の RPC キー上限だけでは足りず、
Zephyr が `custom_settings/<subsystem>/<key>` を 64 byte の名前として保存するためです。
どちらかの上限を超える名前は、node 名を示してビルドが失敗します。

この option には custom Studio RPC protocol を持つ patched ZMK が必要です。その
ため、registry だけでなく `ZMK_CUSTOM_SETTINGS_STUDIO_RPC` に依存します。subsystem
の登録は `zmk/studio/custom.h` を include し、それが取り込む nanopb 生成ヘッダーは
その RPC をビルドするときにしか存在しないからです。

**option を無効にすると(既定)、この部分は何もコンパイルされず、devicetree の値で
固定されます。** processor 自体はこれらに一切依存せず、他の module がない upstream
ZMK でもビルド・動作します。

## プロジェクト構成

```
.
├── CMakeLists.txt
├── Kconfig
├── drivers/input/
│   ├── input_processor_absolute_to_relative.c
│   └── input_processor_absolute_to_relative_custom_settings.c   # only this needs the patched ZMK
├── include/zmk-input-abs2rel/
│   ├── absolute_to_relative_core.h              # dependency-free conversion core
│   ├── absolute_to_relative.h                   # runtime API
│   └── custom_settings.h                        # namespace and key shape
├── tests/
│   ├── integration/                              # upstream and DYA firmware fixtures
│   ├── run-integration-docker.sh
│   ├── run.sh
│   └── test_absolute_to_relative_core.c
├── dts/
│   ├── behaviors/input_processor_absolute_to_relative.dtsi
│   └── bindings/zmk,input-processor-absolute-to-relative.yaml
└── zephyr/module.yml
```

## テスト

`tests/run.sh` は、製品と同じ変換 core を厳しい警告設定でコンパイルし、最適化
ビルド、AddressSanitizer/UndefinedBehaviorSanitizer ビルド、カバレッジ計測で実行します。
CI は純粋な変換 core の行・分岐 100% を要求します。Zephyr 側 module 全体の値ではありません。
最初のサンプルによる基準点の設定、リセット、負方向の対称な丸め、`int32_t` の座標全範囲、
stream の独立性を確認します。

GitHub Actions では、custom settings を無効にした upstream ZMK と、有効にした DYA
fork の両方に対して `tests/integration` もビルドします。fixture では、module の
metadata、Kconfig、Devicetree binding、CMake 連携、1 つの processor を共有する
2 つの listener、`suppress-btn-touch` property を省略した場合を、実際の firmware
target で確認します。DYA ビルドでは、subsystem と代表的な設定キーが firmware に
リンクされていることも確認します。すべての確認は pull request ごとと `main` への
push で実行され、手動でも実行できます。

## License

[MIT](LICENSE)
