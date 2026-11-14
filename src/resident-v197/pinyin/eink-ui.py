"""Apply the Kobo keyboard presentation changes to the pinned PinyinIME tree."""
from pathlib import Path

JAVA = Path("src/com/android/inputmethod/pinyin")


def replace(path, before, after, count=1):
    source = path.read_text()
    if source.count(before) != count:
        raise ValueError(f"Unexpected upstream text in {path}: {before}")
    path.write_text(source.replace(before, after))


ime = JAVA / "PinyinIME.java"
replace(ime, "        return mCandidatesContainer;", """        mFloatingContainer.addView(mCandidatesContainer,
                new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        return mFloatingContainer;""")
# Keep composition in the existing IME window and avoid resizing the app per key.
replace(ime, "        if (null != mSkbContainer) mSkbContainer.requestLayout();\n\n        if (null == mCandidatesContainer)", "        if (null == mCandidatesContainer)")
replace(ime, "        mFloatingWindowTimer.postShowFloatingWindow();\n", "")
container = JAVA / "CandidatesContainer.java"
replace(container, "mFlipper.setInAnimation(mInAnimInUse);", "mFlipper.setInAnimation((Animation) null);")
replace(container, "mFlipper.setOutAnimation(mOutAnimInUse);", "mFlipper.setOutAnimation((Animation) null);")
replace(container, "        mFlipper.showNext();", "        mFlipper.showNext();\n        onAnimationEnd(null);")
replace(JAVA / "SoftKey.java", "return (mKeyMask & KEYMASK_BALLOON) != 0;", "return false;")
keyboard = JAVA / "SoftKeyboardView.java"
replace(keyboard, "invalidate(mDirtyRect);", "// Keep the key surface static during touch.", 4)
replace(keyboard, """        if (mKeyPressed && softKey == mSoftKeyDown) {
            bg = softKey.getKeyHlBg();
            textColor = softKey.getColorHl();
        } else {
            bg = softKey.getKeyBg();
            textColor = softKey.getColor();
        }""", """        bg = softKey.getKeyBg();
        textColor = android.graphics.Color.BLACK;""")
replace(keyboard, "            icon.draw(canvas);", "            icon.setTint(android.graphics.Color.BLACK);\n            icon.draw(canvas);")
candidate = JAVA / "CandidateView.java"
replace(candidate, """                showBalloon(clickedItemInPage, true);
                mTimer.startTimer(BalloonHint.TIME_DELAY_SHOW, mPageNo,
                        clickedItemInPage);""", "                // Selection is applied on release without a preview popup.", 2)

# Flat paper background with black outlines, including the function keys.
drawable = Path("res/drawable")
for name in ("normal_key_bg", "light_key_bg", "light_key_up_bg"):
    (drawable / f"{name}.9.png").unlink()
    (drawable / f"{name}.xml").write_text(
        '<shape xmlns:android="http://schemas.android.com/apk/res/android">'
        '<solid android:color="#ffffff"/><stroke android:width="1dp" android:color="#333333"/>'
        '<corners android:radius="2dp"/></shape>\n'
    )
(drawable / "skb_bg.png").unlink()
(drawable / "skb_bg.xml").write_text(
    '<shape xmlns:android="http://schemas.android.com/apk/res/android">'
    '<solid android:color="#ffffff"/></shape>\n'
)
for file, background in (("skb_container.xml", "skb_container_bg"), ("candidates_container.xml", "cands_container_bg"),
                         ("floating_container.xml", "composing_area_bg")):
    replace(Path("res/layout") / file, "@drawable/" + background, "@android:color/white")

# Backspace in prediction mode must edit the field as well as dismiss suggestions.
replace(ime, "SIMULATE_KEY_DELETE = true", "SIMULATE_KEY_DELETE = false")
replace(ime, """                    ImeState.STATE_PREDICT == mImeState) {
                if (!realAction) return true;
                resetToIdleState(false);
                return true;""", """                    ImeState.STATE_PREDICT == mImeState) {
                if (!realAction) return true;
                resetToIdleState(false);
                InputConnection connection = getCurrentInputConnection();
                if (connection != null) connection.deleteSurroundingText(1, 0);
                return true;""")

replace(ime, "    @Override\n    public void onFinishInputView", """    @Override
    public void onWindowShown() {
        super.onWindowShown();
        try (java.io.FileOutputStream marker = openFileOutput("kobo-keyboard-visible", MODE_PRIVATE)) {
            marker.write(Integer.toString(android.os.Process.myPid()).getBytes("US-ASCII"));
        } catch (java.io.IOException failed) {
            android.util.Log.w(TAG, "Could not publish keyboard visibility", failed);
        }
    }

    @Override
    public void onWindowHidden() {
        deleteFile("kobo-keyboard-visible");
        super.onWindowHidden();
    }

    @Override
    public void onFinishInputView""")

# Keep the selected candidate visible without a dark area that shifts per key.
(drawable / "candidate_hl_bg.9.png").unlink()
(drawable / "candidate_hl_bg.xml").write_text(
    '<shape xmlns:android="http://schemas.android.com/apk/res/android">'
    '<solid android:color="@android:color/transparent"/>'
    '<stroke android:width="1dp" android:color="#000000"/></shape>\n'
)

# Give each page room for larger glyphs and spread even the last page across the row.
replace(JAVA / "Environment.java", "CANDIDATES_AREA_HEIGHT_RATIO_PORTRAIT = 0.084f",
        "CANDIDATES_AREA_HEIGHT_RATIO_PORTRAIT = 0.10f")
replace(candidate, "while (xPos < mContentWidth && pStart + pSize < candSize)",
        "while (pSize < 10 && xPos < mContentWidth && pStart + pSize < candSize)")
replace(candidate, """            float marginExtra = (mContentWidth - xPos) / pSize / 2;""",
        """            float marginExtra = pSize > 0 ? Math.max(0, mContentWidth - xPos) / pSize / 2 : 0;""")
source = candidate.read_text()
start = source.index("            if (mContentWidth - xPos > lastItemWidth)")
end = source.index("            mCandidateMarginExtra = marginExtra;", start)
candidate.write_text(source[:start] + source[end:])
replace(candidate, "            float lastItemWidth = 0;\n", "")
replace(candidate, "                    lastItemWidth = itemWidth;\n", "")

# Short taps may end before an e-ink refresh. Keep a small mark visible on release.
replace(keyboard, "    public SoftKey onKeyRelease(int x, int y) {", """    private final Rect mKeyFeedback = new Rect();
    private final Paint mKeyFeedbackPaint = new Paint();
    private final Runnable clearKeyFeedback = new Runnable() {
        public void run() {
            invalidate(mKeyFeedback);
            mKeyFeedback.setEmpty();
        }
    };

    public SoftKey onKeyRelease(int x, int y) {""")
replace(keyboard, """        if (mSoftKeyDown.moveWithinKey(x - getPaddingLeft(), y - getPaddingTop())) {
            return mSoftKeyDown;
        }
        return null;""", """        if (mSoftKeyDown.moveWithinKey(x - getPaddingLeft(), y - getPaddingTop())) {
            removeCallbacks(clearKeyFeedback);
            invalidate(mKeyFeedback);
            int bottom = getPaddingTop() + mSoftKeyDown.mBottom - 6;
            mKeyFeedback.set(getPaddingLeft() + mSoftKeyDown.mLeft + 10, bottom - 4,
                    getPaddingLeft() + mSoftKeyDown.mRight - 10, bottom);
            invalidate(mKeyFeedback);
            postDelayed(clearKeyFeedback, 450);
            return mSoftKeyDown;
        }
        return null;""")
replace(keyboard, """        mDirtyRect.setEmpty();
    }

    private void drawSoftKey""", """        if (!mKeyFeedback.isEmpty()) {
            mKeyFeedbackPaint.setColor(android.graphics.Color.BLACK);
            canvas.save();
            canvas.translate(-getPaddingLeft(), -getPaddingTop());
            canvas.drawRect(mKeyFeedback, mKeyFeedbackPaint);
            canvas.restore();
        }
        mDirtyRect.setEmpty();
    }

    private void drawSoftKey""")
