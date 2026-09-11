# ZMK Absolute-to-Relative Input Processor

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

複数の変換器が必要な場合や node 名を自分で決める場合は、互換性名を指定して
node を宣言できます。

```dts
pointer_abs_rel: pointer_abs_rel {
    compatible = "zmk,input-processor-absolute-to-relative";
    #input-processor-cells = <0>;
    suppress-btn-touch;
};
```

### 標準 node

module は pointer 用と scroll 用の 2 node を提供します。

| 参照 label | 実 node 名 | ボタン処理 |
| :--- | :--- | :--- |
| `zip_absolute_to_relative` | `abs_rel` | `BTN_TOUCH` を抑止し、`BTN_0` のクリックは通す |
| `zip_absolute_to_relative_scroll` | `abs_rel_scroll` | scroll 用。`BTN_TOUCH` と `BTN_0` の両方を抑止 |

通常 pointer は前者、クリックをホストへ送らない scroll 経路は後者を使います。

### 設定プロパティ

| プロパティ | 型 | 既定値 | 説明 |
| :--- | :--- | :--- | :--- |
| `suppress-btn-touch` | bool | module 標準 node は true。手動宣言 node は false | 基準点の更新に使った `INPUT_BTN_TOUCH` を下流の HID へ送らない |
| `suppress-btn0` | bool | false | トラックパッドがクリックとして送る `INPUT_BTN_0` を抑止する |

module の標準 node では `suppress-btn-touch` が有効です。手動で node を宣言する
場合にのみ、接触状態を意図的に下流へ渡すために省略できます。

`suppress-btn0` を有効にした後で無効へ変更しても、すでに抑止した press の
release は戻りません。release だけを抑止すると、ホスト上でボタンが押しっぱなしに
なるためです。

## 動作

### 平滑化

各軸の移動量は、現在の差分と直前の差分の平均です。

```text
smooth = (current_delta + previous_delta) / 2
```

各軸の最初のサンプルは基準点を作るだけで、移動イベントを出しません。

### 基準点と layer 変更

`BTN_TOUCH` の press / release と layer 変更時に基準点を破棄します。layer により
input processor chain が変わる途中で古い基準点を残すと、別の接触との距離を移動量と
して扱い、ポインターが跳ぶ原因になるためです。

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
│   ├── absolute_to_relative.h
│   └── custom_settings.h
└── zephyr/module.yml
```

## License

MIT
