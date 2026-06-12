# kg_LayoutGrid AEX

[日本語](#日本語) / [English](#english)

## 日本語

`kg_LayoutGrid` は、アクティブなコンポジション向けに Figma ライクなストレッチレイアウトグリッドを作成する After Effects プラグインです。

この `.aex` には2つの登録が含まれます。

- AEGPコマンド: `Window > kg_LayoutGrid...`
- PFエフェクト: `Effect > KG_plugins > kg_LayoutGrid`

主なワークフローはPFエフェクトです。エフェクトを適用したレイヤー上にレイアウトグリッドを直接描画し、そのレイヤーのコメントに生成済みガイドの履歴を保存してコントローラーとしても使います。

### ビルド

- ターゲット: Windows x64 `.aex`
- ツールチェーン: Visual Studio 2022、MSVC v143
- SDK配置: このリポジトリから見た `Examples/Headers`、`Examples/Util`、`Examples/Resources/PiPLTool`

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Examples\kg_plugins\kg_LayoutGrid\Win\kg_LayoutGrid.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

出力先:

```text
Examples/kg_plugins/build/Release/kg_LayoutGrid.aex
```

インストールするには、生成された `.aex` をAfter EffectsのPlug-insフォルダーにコピーします。

```text
C:\Program Files\Adobe\Adobe After Effects 2025\Support Files\Plug-ins\
```

### 使い方

1. `Effect > KG_plugins > kg_LayoutGrid` を平面または任意のレイヤーに適用します。
2. Effect Controlsで Columns、Rows、Band Opacity、Column Color、Row Color、Spiral Color を設定します。
3. `Apply Guides` で現在の設定からAE標準ガイドを作成します。
4. `Clear Grid Guides` または `Clear All` で、このエフェクトが作成したガイドを削除します。


### 既知の制限

- Windowメニュー配下のコマンドユーティリティであり、ドッキング可能なAEパネルではありません。
- 適用されたエフェクトは対象レイヤーに透明なグリッドバンドを描画します。別のプレビューレイヤーは作成しません。
- AE標準ガイドはExpressionでライブ制御されません。
- AE標準ガイドを作るには `Apply Guides` が必要です。
- AE標準ガイドには名前がないため、削除時は `orientation + position` の一致で判定します。
- 手動で作成したガイドが、生成されたガイドと完全に同じ向き・位置にある場合、AE側で区別できません。
- macOSは未実装です。AEGPエントリーとグリッドロジックは移植可能ですが、UIにはmacOS向け実装が必要です。
- ガイド生成が上手く行かない問題を確認しています。

### ライセンス

このプラグインはリポジトリルートの [LICENSE](../LICENSE) に記載された Apache License 2.0 の下で公開されています。

## English

`kg_LayoutGrid` is an After Effects SDK native plug-in that creates a Figma-like stretch layout grid for the active composition.

The `.aex` contains two registrations:

- AEGP command: `Window > kg_LayoutGrid...`
- PF effect: `Effect > KG_plugins > kg_LayoutGrid`

The primary workflow is the PF effect. It draws the layout grid directly on the layer it is applied to, and the applied layer also acts as the controller by storing generated guide history in its layer comment.

### Build

- Target: Windows x64 `.aex`
- Toolchain: Visual Studio 2022, MSVC v143
- SDK layout: this repository's `Examples/Headers`, `Examples/Util`, and `Examples/Resources/PiPLTool`

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Examples\kg_plugins\kg_LayoutGrid\Win\kg_LayoutGrid.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Output:

```text
Examples/kg_plugins/build/Release/kg_LayoutGrid.aex
```

Install by copying the generated `.aex` to an After Effects Plug-ins folder.

```text
C:\Program Files\Adobe\Adobe After Effects 2025\Support Files\Plug-ins\
```

### Use

1. Apply `Effect > KG_plugins > kg_LayoutGrid` to a solid or layer.
2. Set Columns, Rows, Band Opacity, Column Color, Row Color, and Spiral Color in Effect Controls.
3. Use `Apply Guides` to create standard After Effects guides from the current settings.
4. Use `Clear Grid Guides` or `Clear All` to remove guides created by this effect.

The Window menu command remains available as a utility dialog, but the primary workflow is now the Effect Controls UI.

### Implementation Notes

The main tool is a native AEGP command plug-in with a small Win32 dialog. AE composition changes are executed through `AEGP_ExecuteScript`. This is intentional because this SDK does not expose a stable direct AEGP suite for standard After Effects guide creation/removal, while ExtendScript provides `comp.addGuide()` and `comp.removeGuide()`.

A PF effect entry point renders transparent column and row bands. Overlapping row/column areas become darker because the bands are alpha-composited. Guide operations still use `AEGP_ExecuteScript` because standard After Effects guides are not exposed through a stable direct AEGP suite.

Grid math and validation are plain C++ in `src/GridMath.*` and `src/GridValidation.*`.

### Known Limits

- This is a command utility under the Window menu, not a dockable AE panel.
- The applied effect draws transparent grid bands on the target layer; it does not create a separate preview layer.
- Standard After Effects guides are not live Expression-controlled.
- Standard guides require `Apply Guides`.
- Standard After Effects guides have no names, so cleanup matches `orientation + position`.
- If a manual guide exists at exactly the same orientation and position as a generated guide, After Effects cannot distinguish it.
- macOS is not implemented yet; the AEGP entry and grid logic are portable, but UI needs a macOS implementation.

### License

This plug-in is released under the Apache License 2.0 described in the repository root [LICENSE](../LICENSE).
