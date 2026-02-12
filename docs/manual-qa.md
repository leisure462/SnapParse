# SnapParse Manual QA Checklist

## 1. Selection and Action Bar

- Drag select a sentence in browser, editor, and document viewer.
- Verify floating action bar appears near cursor.
- Double-click a word and verify action bar appears.
- Click outside selected content and verify action bar hides.

## 2. Action Behavior

- Click `翻译`: action bar hides first, translate popup opens near selection and receives selected text.
- Click `总结`: action bar hides first, summary popup opens near selection and receives selected text.
- Click `解释`: action bar hides first, explain popup opens near selection and receives selected text.
- Click `搜索`: browser search opens with selected text query.
- Click `复制`: selected text is copied to clipboard.

## 3. Window UX

- Verify window header controls are visible (置顶 / 透明度 / 最小化 / 关闭).
- Verify `显示原文` toggle works in translate and summary windows.
- Verify loading state appears before result.

## 4. Settings Sidebar

- Open settings window.
- Verify first selected section is `API配置`.
- Verify sidebar order:
  - API配置
  - 工具栏
  - 功能窗口
  - 功能
  - 高级设置
- In `工具栏`, verify `默认主题模式` selector exists.

## 5. Theme Persistence

- Change `默认主题模式` in settings.
- Verify visible window theme changes immediately.
- Open settings window and verify selected theme mode matches.
- Restart app and verify theme mode persists.

## 6. API Integration

- Fill API config with valid OpenAI-compatible endpoint.
- Click `测试 API` and verify the status shows success with latency/model info.
- Save settings and run translate/summarize/explain.
- Verify responses are returned from model.

## 7. Negative Cases

- Empty selection should not produce invalid output.
- Invalid API key should display clear error message.
- Unsupported app selection should fail gracefully without crash.
