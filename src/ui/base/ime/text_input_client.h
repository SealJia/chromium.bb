// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_CLIENT_H_
#define UI_BASE_IME_TEXT_INPUT_CLIENT_H_

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/strings/string16.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/ui_base_ime_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"

namespace gfx {
class Rect;
}

namespace ui {

class KeyEvent;

// An interface implemented by a View that needs text input support.
class UI_BASE_IME_EXPORT TextInputClient {
 public:
  virtual ~TextInputClient();

  // Input method result -------------------------------------------------------

  // Sets composition text and attributes. If there is composition text already,
  // it'll be replaced by the new one. Otherwise, current selection will be
  // replaced. If there is no selection, the composition text will be inserted
  // at the insertion point.
  virtual void SetCompositionText(const ui::CompositionText& composition) = 0;

  // Converts current composition text into final content.
  virtual void ConfirmCompositionText() = 0;

  // Removes current composition text.
  virtual void ClearCompositionText() = 0;

  // Inserts a given text at the insertion point. Current composition text or
  // selection will be removed. This method should never be called when the
  // current text input type is TEXT_INPUT_TYPE_NONE.
  virtual void InsertText(const base::string16& text) = 0;

  // Inserts a single char at the insertion point. Unlike above InsertText()
  // method, this method takes an |event| parameter indicating
  // the event that was unprocessed. This method should only be
  // called when a key press is not handled by the input method but still
  // generates a character (eg. by the keyboard driver). In another word, the
  // preceding key press event should not be a VKEY_PROCESSKEY.
  // This method will be called whenever a char is generated by the keyboard,
  // even if the current text input type is TEXT_INPUT_TYPE_NONE.
  virtual void InsertChar(const ui::KeyEvent& event) = 0;

  // Input context information -------------------------------------------------

  // Returns current text input type. It could be changed and even becomes
  // TEXT_INPUT_TYPE_NONE at runtime.
  virtual ui::TextInputType GetTextInputType() const = 0;

  // Returns current text input mode. It could be changed and even becomes
  // TEXT_INPUT_MODE_DEFAULT at runtime.
  virtual ui::TextInputMode GetTextInputMode() const = 0;

  // Returns the current text input flags, which is a bit map of
  // WebTextInputType defined in blink. This is valid only for web input fileds;
  // it will return TEXT_INPUT_FLAG_NONE for native input fields.
  virtual int GetTextInputFlags() const = 0;

  // Returns if the client supports inline composition currently.
  virtual bool CanComposeInline() const = 0;

  // Returns current caret (insertion point) bounds in the universal screen
  // coordinates. If there is selection, then the selection bounds will be
  // returned.
  // Note: On Windows, the returned value is supposed to be DIP (Density
  // Independent Pixel).
  // TODO(ime): Have a clear spec whether the returned value is DIP or not.
  // http://crbug.com/360334
  virtual gfx::Rect GetCaretBounds() const = 0;

  // Retrieves the composition character boundary rectangle in the universal
  // screen coordinates. The |index| is zero-based index of character position
  // in composition text.
  // Returns false if there is no composition text or |index| is out of range.
  // The |rect| is not touched in the case of failure.
  // Note: On Windows, the returned value is supposed to be DIP
  // (Density Independent Pixel).
  // TODO(ime): Have a clear spec whether the returned value is DIP or not.
  // http://crbug.com/360334
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) const = 0;

  // Returns true if there is composition text.
  virtual bool HasCompositionText() const = 0;

  // Document content operations ----------------------------------------------

  // Retrieves the UTF-16 based character range containing accessible text in
  // the View. It must cover the composition and selection range.
  // Returns false if the information cannot be retrieved right now.
  virtual bool GetTextRange(gfx::Range* range) const = 0;

  // Retrieves the UTF-16 based character range of current composition text.
  // Returns false if the information cannot be retrieved right now.
  virtual bool GetCompositionTextRange(gfx::Range* range) const = 0;

  // Retrieves the UTF-16 based character range of current selection.
  // Returns false if the information cannot be retrieved right now.
  virtual bool GetSelectionRange(gfx::Range* range) const = 0;

  // Selects the given UTF-16 based character range. Current composition text
  // will be confirmed before selecting the range.
  // Returns false if the operation is not supported.
  virtual bool SetSelectionRange(const gfx::Range& range) = 0;

  // Deletes contents in the given UTF-16 based character range. Current
  // composition text will be confirmed before deleting the range.
  // The input caret will be moved to the place where the range gets deleted.
  // ExtendSelectionAndDelete should be used instead as far as you are deleting
  // characters around current caret. This function with the range based on
  // GetSelectionRange has a race condition due to asynchronous IPCs between
  // browser and renderer.
  // Returns false if the operation is not supported.
  virtual bool DeleteRange(const gfx::Range& range) = 0;

  // Retrieves the text content in a given UTF-16 based character range.
  // The result will be stored into |*text|.
  // Returns false if the operation is not supported or the specified range
  // is out of the text range returned by GetTextRange().
  virtual bool GetTextFromRange(const gfx::Range& range,
                                base::string16* text) const = 0;

  // Miscellaneous ------------------------------------------------------------

  // Called whenever current keyboard layout or input method is changed,
  // especially the change of input locale and text direction.
  virtual void OnInputMethodChanged() = 0;

  // Called whenever the user requests to change the text direction and layout
  // alignment of the current text box. It's for supporting ctrl-shift on
  // Windows.
  // Returns false if the operation is not supported.
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) = 0;

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret. This function should be used
  // instead of calling DeleteRange with GetSelectionRange, because
  // GetSelectionRange may not be the latest value due to asynchronous of IPC
  // between browser and renderer.
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) = 0;

  // Ensure the caret is within |rect|.  |rect| is in screen coordinates and
  // may extend beyond the bounds of this TextInputClient.
  // Note: On Windows, the returned value is supposed to be DIP (Density
  // Independent Pixel).
  // TODO(ime): Have a clear spec whether the returned value is DIP or not.
  // http://crbug.com/360334
  virtual void EnsureCaretInRect(const gfx::Rect& rect) = 0;

  // Returns true if |command_id| is currently allowed to be executed.
  virtual bool IsEditCommandEnabled(int command_id) = 0;

  // Execute the command specified by |command_id| on the next key event.
  // This allows a TextInputClient to be informed of a platform-independent edit
  // command that has been derived from the key event currently being dispatched
  // (but not yet sent to the TextInputClient). The edit command will take into
  // account any OS-specific, or user-specified, keybindings that may be set up.
  virtual void SetEditCommandForNextKeyEvent(int command_id) = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_CLIENT_H_
