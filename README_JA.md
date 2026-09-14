# ZMK Absolute-to-Relative Input Processor

[![Test](https://github.com/amgskobo/zmk-input-abs2rel/actions/workflows/test.yml/badge.svg)](https://github.com/amgskobo/zmk-input-abs2rel/actions/workflows/test.yml)

[English](README.md)

絶対座標を報告するトラックパッドやタッチセンサーの座標を、相対的な
ポインター移動へ変換する ZMK input processor です。現在値と直前の値の
差分を取り、2 サンプルで平滑化します。つまり「指がどこにあるか」ではなく
「どれだけ動いたか」を必要とするポインター処理チェーンへ接続できます。

## この module が独立している理由

ZMK 標準には scaler、transform、code mapper、temp layer などの input
processor があります。本 processor は絶対座標を相対座標へ変換する専用機能で、
それらの置き換えではありません。`zmk-input-processors` が提供する runtime
processor と同様に、DTS の node 名を設定キーとして使えます。

## 導入

`config/west.yml` の manifest に追加します。

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

module の標準 node を使う場合は DTS を include します。

```dts
#include <behaviors/input_processor_absolute_to_relative.dtsi>

&trackpad_listener {
    input-processors = <&zip_absolute_to_relative>;
};
```

異なるボタン抑止方針が必要な場合や、node 名を自分で決める場合は、互換性名を指定して
node を宣言できます。

```dts
pointer_abs_rel: pointer_abs_rel {
    compatible = "zmk,input-processor-absolute-to-relative";
    #input-processor-cells = <0>;
};
```

`INPUT_BTN_TOUCH` の抑止は、手動宣言nodeを含めて既定で有効です。既存overlayとの
互換性と設定方針の明示用に、`suppress-btn-touch` propertyも引き続き受け付けます。

Devicetreeのboolean propertyは、propertyが存在することで`true`を表し、`false`の値を
持てません。そのためruntime custom settingsを使わないbuildでは`BTN_TOUCH`は常に
抑止されます。`false`へ切り替える必要がある場合は、後述のcustom settings optionを
有効にしてください。

### 標準 node

module は pointer 用と scroll 用の計 2 node を提供します。各nodeはZMKの
input listenerごとに独立した変換状態を持つため、local deviceとsplit proxy deviceで
同じnodeを安全に共有できます。

| 参照 label | 実 node 名 | ボタン処理 |
| :--- | :--- | :--- |
| `zip_absolute_to_relative` | `abs_rel` | `BTN_TOUCH` を抑止し、`BTN_0` のクリックは通す |
| `zip_absolute_to_relative_scroll` | `abs_rel_scroll` | scroll 用。`BTN_TOUCH` と `BTN_0` の両方を抑止 |

通常 pointer は前者、クリックをホストへ送らない scroll 経路は後者を使います。

変換状態はprocessor nodeと`input_device_index`の組に属します。ZMKからinput listenerの
instance indexが渡されるため、同じnodeを使う複数listenerでも、基準座標、平滑化履歴、
抑止済みbuttonの記録は分離されます。runtime設定はnodeに属し、そのnodeを使うlistenerで
共有されます。異なるボタン方針または個別の設定項目が必要な場合だけ、短い固有名を持つ
nodeを追加してください。

### 設定プロパティ

| プロパティ | 型 | 既定値 | 説明 |
| :--- | :--- | :--- | :--- |
| `suppress-btn-touch` | bool | true | 基準点の更新に使った `INPUT_BTN_TOUCH` を抑止し、ZMKでmouse button 0として扱われないようにする |
| `suppress-btn0` | bool | false | トラックパッドがクリックとして送る `INPUT_BTN_0` を抑止する |

`suppress-btn-touch` は、module の標準 node と手動宣言 node のどちらでも既定で
有効です。接触状態を意図的に下流へ渡す場合は、runtime settings で無効にします。

`suppress-btn0` を有効にした後で無効へ変更しても、抑止済みpressをhostへ後から
送信することはありません。続くreleaseは安全のため通過させます。そのpressをhostは
受け取っていないため、通常このreleaseは何も解除しません。逆に、hostへ届いたpressの
releaseだけを抑止すると、buttonが押しっぱなしになるためです。

runtime設定で `suppress-btn-touch` を無効にした場合、通過するtouch edgeはそれ自体を
sync境界にします。edge直後の最初の座標pairは基準点として消費されるため、別のsyncが
なければreleaseがhostへ送られず、mouse button 0が押されたままになるからです。

## 動作

### 平滑化

各軸の移動量は、現在の差分と直前の差分の平均です。

```text
smooth = (current_delta + previous_delta) / 2
```

各軸の最初のサンプルは基準点を作るだけで、移動イベントを出しません。

### 基準点と layer 変更

基準点はinput listenerごとに独立しています。`BTN_TOUCH` の press / releaseでは、その
listenerの基準点だけを破棄します。layer変更時は全listenerの基準点を無効化します。
layer によりinput processor chain が変わる途中で古い基準点を残すと、別の接触との距離を
移動量として扱い、ポインターが跳ぶ原因になるためです。

layer変更callbackは変換状態を直接変更せず、共有atomic generation counterだけを進めます。
各listenerのstreamは次のeventで新しいgenerationを適用します。input threadはevent処理の
前後でもgenerationを比較し、途中で変わったeventを破棄してから新しい基準点を作ります。
これによりlayer callbackと座標変換が同じ状態へ同時に書き込むことを避けます。

### 座標範囲

Zephyr `input_event.value` のsigned `int32_t` 全域を保持します。2座標間の差がその範囲を
超える場合はwrapさせず、平滑化前に `INT32_MIN` または `INT32_MAX` へ飽和します。
下流へ渡すrelative eventも `int32_t` です。

## DYA Studio での実行時設定

`CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS=y` を有効にすると、
[zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings)
を通じて DYA Studio の設定一覧に公開されます。subsystem 名は
`amgskobo__a2r` です。

設定キーは実 node 名とフィールド名を `.` で連結したものです。

```text
abs_rel.suppress_btn_touch
abs_rel_scroll.suppress_btn0
```

### 名前の長さ制限

RPC key は最大 47 文字、Zephyr settings の永続名は最大 63 文字です。永続名は
次の形で保存されます。

```text
custom_settings/<subsystem>/<node>.<field>
```

`amgskobo__a2r` では最長フィールド `suppress_btn_touch` が 18 文字のため、
node 名は最大 **14 文字**です。`abs_rel_scroll` は 14 文字で、この上限に
ちょうど収まります。制限を超える node 名は build 時の `BUILD_ASSERT` で失敗します。

この設定機能には custom Studio RPC を含む patched ZMK が必要です。設定機能を
無効にすれば、processor 自体は upstream ZMK でも動作し、DTS 値は固定になります。

## 構成

```text
.
├── drivers/input/
│   ├── input_processor_absolute_to_relative.c
│   └── input_processor_absolute_to_relative_custom_settings.c
├── dts/
│   ├── behaviors/input_processor_absolute_to_relative.dtsi
│   └── bindings/zmk,input-processor-absolute-to-relative.yaml
├── include/zmk-input-abs2rel/
│   ├── absolute_to_relative_core.h
│   ├── absolute_to_relative.h
│   └── custom_settings.h
├── tests/
│   ├── integration/                 # upstream / DYA firmware fixture
│   ├── run-integration-docker.sh
│   ├── run.sh
│   └── test_absolute_to_relative_core.c
└── zephyr/module.yml
```

## テスト

`tests/run.sh` はproductionと同じ変換coreを厳格なcompiler warning設定でbuildし、通常の
最適化buildとAddressSanitizer／UndefinedBehaviorSanitizer buildの両方を実行します。
最初のsample、reset、正負対称の丸め、`int32_t` 全域、独立streamを検証します。

GitHub Actionsではさらに、custom settingsを無効にしたupstream ZMKと、有効にした
DYA forkの両方に対して`tests/integration` のfirmware fixtureをbuildします。module
metadata、Kconfig、Devicetree binding、CMake integrationに加えて、
`suppress-btn-touch`を省略した場合も実機firmware targetでcompileできることを確認します。
DYA buildではsubsystemと代表的なsettings keyがfirmwareへlinkされたことも検証します。
すべてのcheckをpush、pull request、手動実行で利用できます。

## License

[MIT](LICENSE)
