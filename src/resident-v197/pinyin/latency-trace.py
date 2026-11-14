"""Add opt-in timing probes to a diagnostics build of the pinned PinyinIME."""
from pathlib import Path

java = Path("src/com/android/inputmethod/pinyin")


def replace(path, before, after):
    source = path.read_text()
    if source.count(before) != 1:
        raise ValueError(f"Unexpected upstream text in {path}: {before}")
    path.write_text(source.replace(before, after))


keyboard = java / "SkbContainer.java"
replace(keyboard, "\n                responseKeyEvent(mSoftKeyDown);", """\n                if (mSoftKeyDown != null) {
                    android.util.Log.i("IME_LATENCY", "ime_up_ms=" + android.os.SystemClock.uptimeMillis()
                            + " event_ms=" + event.getEventTime());
                }
                responseKeyEvent(mSoftKeyDown);""")

ime = java / "PinyinIME.java"
replace(ime, "                int totalChoicesNum = 0;", """                int totalChoicesNum = 0;
                long begin = android.os.SystemClock.uptimeMillis();""")
replace(ime, "                updateDecInfoForSearch(totalChoicesNum);", """                long decoded = android.os.SystemClock.uptimeMillis();
                updateDecInfoForSearch(totalChoicesNum);
                long ready = android.os.SystemClock.uptimeMillis();
                android.util.Log.i("IME_LATENCY", "decoder_begin_ms=" + begin
                        + " decoder_ms=" + (decoded - begin) + " prepare_ms=" + (ready - decoded)
                        + " ready_ms=" + ready + " count=" + mCandidatesList.size());""")
