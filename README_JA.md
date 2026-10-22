# zmk-input-abs2rel

絶対座標を相対移動へ変換し、2サンプルで平滑化する小型 ZMK モジュールです。
入力元ごとの基準座標、差分・丸め、接触やレイヤー変更時の基準リセットを所有します。
元のボタンは type/code/value/sync を保って後段へ渡します。

## 共通最終段とのセット構成

ホスト向けの抑止は、別の `zmk-input-suppress` が所有します。すべての経路の末尾に
同じ抑止器を置き、scroll も同じ converter と wheel mapper、末尾の抑止マスク を使います。
標準で有効なのは `zip_absolute_to_relative` だけです。pointerとscrollで同じconverterを使います。
旧 `zip_absolute_to_relative_scroll` は独立履歴のインスタンスとして残しますが、既定では無効です。
既存設定で旧ラベルを使う場合は共通converterへ変更するか、独立履歴が必要なら次のように明示的に有効化します。

```dts
&zip_absolute_to_relative_scroll { status = "okay"; };
```

初回基準や古い座標はsyncを保つ中立MSC_SCANになります。ボタン方針はconverterに持たせません。

旧 `suppress-btn-touch` / `suppress-btn0` と抑止 API は削除しました。
旧 `CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS` も削除します。
共通抑止器にプロファイル表・レイヤー監視・旧設定名の互換処理はありません。
旧保存値は消去しませんが読み込み・適用もしません。DTSの経路末尾にマスクを直接渡します。
構成例は [README.md](README.md)、検証と移行対応は [tests/README.md](tests/README.md)
を参照してください。

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
適用し、処理中に generation が変わった場合は古い座標を中立化し、元のボタンを通して
新しい状態から始めます。これにより、レイヤーの callback と座標変換が同じ状態へ
同時に書き込むことはありません。

### 値の範囲

変換 core は、Zephyr の input event が使う符号付き `int32_t` の全範囲を保持します。
その範囲を超える差分は、折り返さずに平滑化の前で飽和させます。出力は、下流の
processor と listener に向けた `int32_t` の相対イベントのままです。
