/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.text.cts;

import android.test.AndroidTestCase;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.text.Layout.Alignment;
import android.text.style.MetricAffectingSpan;

public class StaticLayoutLineBreakingTest extends AndroidTestCase {
    // Span test are currently not supported because text measurement uses the MeasuredText
    // internal mWorkPaint instead of the provided MockTestPaint.
    private static final boolean SPAN_TESTS_SUPPORTED = false;

    private static final float SPACE_MULTI = 1.0f;
    private static final float SPACE_ADD = 0.0f;
    private static final int WIDTH = 100;
    private static final Alignment ALIGN = Alignment.ALIGN_LEFT;

    final static char SURR_FIRST = '\uD800';
    final static char SURR_SECOND = '\uDF31';

    private static final int[] NO_BREAK = new int[] {};

    private static final TextPaint mTextPaint = new MockTextPaint();

    private static class MockTextPaint extends TextPaint {

        @Override
        public float getTextRunAdvances(char[] chars, int index, int count,
                int contextIndex, int contextCount, int flags, float[] advances,
                int advancesIndex, int reserved) {

            // Conditions copy pasted from Paint
            if (chars == null) {
                throw new IllegalArgumentException("text cannot be null");
            }

            if (flags != DIRECTION_LTR && flags != DIRECTION_RTL) {
                throw new IllegalArgumentException("unknown flags value: " + flags);
            }

            if ((index | count | contextIndex | contextCount | advancesIndex
                    | (index - contextIndex) | (contextCount - count)
                    | ((contextIndex + contextCount) - (index + count))
                    | (chars.length - (contextIndex + contextCount))
                    | (advances == null ? 0 :
                        (advances.length - (advancesIndex + count)))) < 0) {
                throw new IndexOutOfBoundsException();
            }

            float res = 0.0f;

            if (advances != null) {
                for (int i = 0; i < count; i++) {
                    float width = getCharWidth(chars[index + i]);
                    advances[advancesIndex + i] = width;
                    res += width;
                }
            }

            return res;
        }
    }

    private static float getCharWidth(char c) {
        switch (Character.toUpperCase(c)) {
            // Roman figures
            case 'I': return 1.0f;
            case 'V': return 5.0f;
            case 'X': return 10.0f;
            case 'L': return 50.0f;
            case 'C': return 100.0f; // equals to WIDTH
            case ' ': return 10.0f;
            case '.': return 0.0f; // 0-width character
            case SURR_FIRST: return 7.0f;
            case SURR_SECOND: return 3.0f; // Sum of SURR_FIRST-SURR_SECOND is 10
            default: return 10.0f;
        }
    }

    private static StaticLayout getStaticLayout(CharSequence source, int width) {
        return new StaticLayout(source, mTextPaint, width, ALIGN, SPACE_MULTI, SPACE_ADD, false);
    }

    private static StaticLayout getStaticLayout(CharSequence source) {
        return getStaticLayout(source, WIDTH);
    }

    private static int[] getBreaks(StaticLayout staticLayout) {
        int[] breaks = new int[staticLayout.getLineCount() - 1];
        for (int line = 0; line < breaks.length; line++) {
            breaks[line] = staticLayout.getLineEnd(line);
        }
        return breaks;
    }

    private static void layout(CharSequence source, int[] breaks) {
        StaticLayout staticLayout = getStaticLayout(source);
        layout(staticLayout, source, breaks);
    }

    private static void layout(StaticLayout staticLayout, CharSequence source, int[] breaks) {
        //Log.i("StaticLayoutLineWrappingTest", "String " + source.toString() + "; " +
        //        staticLayout.getLineCount() + " lines");

        int lineCount = breaks.length + 1;
        assertEquals("Number of lines", lineCount, staticLayout.getLineCount());

        for (int line = 0; line < lineCount; line++) {
            int lineStart = staticLayout.getLineStart(line);
            int lineEnd = staticLayout.getLineEnd(line);

            //Log.i("StaticLayoutLineWrappingTest", "Line " + line + " [" + lineStart + ".." +
            //        lineEnd + "]\t" + source.subSequence(lineStart, lineEnd));

            if (line == 0) {
                assertEquals("Line start for first line", 0, lineStart);
            } else {
                assertEquals("Line start for line " + line, breaks[line - 1], lineStart);
            }

            if (line == lineCount - 1) {
                assertEquals("Line end for last line", source.length(), lineEnd);
            } else {
                assertEquals("Line end for line " + line, breaks[line], lineEnd);
            }
        }
    }

    final static int MAX_SPAN_COUNT = 10;
    final static int[] spanStarts = new int[MAX_SPAN_COUNT];
    final static int[] spanEnds = new int[MAX_SPAN_COUNT];

    private static MetricAffectingSpan getMetricAffectingSpan() {
        return new MetricAffectingSpan() {
            @Override
            public void updateDrawState(TextPaint tp) { /* empty */ }

            @Override
            public void updateMeasureState(TextPaint p) { /* empty */ }
        };
    }

    /**
     * Replaces the "<...>" blocks by spans, assuming non overlapping, correctly defined spans
     * @param text
     * @return A CharSequence with '<' '>' replaced by MetricAffectingSpan
     */
    private static CharSequence spanify(String text) {
        int startIndex = text.indexOf('<');
        if (startIndex < 0) return text;

        int spanCount = 0;
        do {
            int endIndex = text.indexOf('>');
            if (endIndex < 0) throw new IllegalArgumentException("Unbalanced span markers");

            text = text.substring(0, startIndex) + text.substring(startIndex + 1, endIndex) +
                    text.substring(endIndex + 1);

            spanStarts[spanCount] = startIndex;
            spanEnds[spanCount] = endIndex - 2;
            spanCount++;

            startIndex = text.indexOf('<');
        } while (startIndex >= 0);

        SpannableStringBuilder result = new SpannableStringBuilder(text);
        for (int i = 0; i < spanCount; i++) {
            result.setSpan(getMetricAffectingSpan(), spanStarts[i], spanEnds[i],
                    Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        }
        return result;
    }

    public void testNoLineBreak() {
        // Width lower than WIDTH
        layout("", NO_BREAK);
        layout("I", NO_BREAK);
        layout("V", NO_BREAK);
        layout("X", NO_BREAK);
        layout("L", NO_BREAK);
        layout("I VILI", NO_BREAK);
        layout("XXXX", NO_BREAK);
        layout("LXXXX", NO_BREAK);

        // Width equal to WIDTH
        layout("C", NO_BREAK);
        layout("LL", NO_BREAK);
        layout("L XXXX", NO_BREAK);
        layout("XXXXXXXXXX", NO_BREAK);
        layout("XXX XXXXXX", NO_BREAK);
        layout("XXX XXXX X", NO_BREAK);
        layout("XXX XXXXX ", NO_BREAK);
        layout(" XXXXXXXX ", NO_BREAK);
        layout("  XX  XXX ", NO_BREAK);
        //      0123456789

        // Width greater than WIDTH, but no break
        layout("  XX  XXX  ", NO_BREAK);
        layout("XX XXX XXX ", NO_BREAK);
        layout("XX XXX XXX     ", NO_BREAK);
        layout("XXXXXXXXXX     ", new int[] {10}); // Bug, should be NO_BREAK as above
        //      01234567890
    }

    public void testOneLineBreak() {
        //      01234567890
        layout("XX XXX XXXX", new int[] {7});
        layout("XX XXXX XXX", new int[] {8});
        layout("XX XXXXX XX", new int[] {9});
        layout("XX XXXXXX X", new int[] {10});
        //      01234567890
        layout("XXXXXXXXXXX", new int[] {10});
        layout("XXXXXXXXX X", new int[] {10});
        layout("XXXXXXXX XX", new int[] {9});
        layout("XXXXXXX XXX", new int[] {8});
        layout("XXXXXX XXXX", new int[] {7});
        //      01234567890
        layout("LL LL", new int[] {2, 3}); // Bug: should be {3}
        layout("LLLL", new int[] {2});
        layout("C C", new int[] {1, 2}); // Bug: should be {2}
        layout("CC", new int[] {1});
    }

    public void testSpaceAtBreak() {
        //      0123456789012
        layout("XXXX XXXXX X", new int[] {11});
        layout("XXXXXXXXXX X", new int[] {10}); // Bug: should be {11}. Consume spaces in the non ok case too
        layout("XXXXXXXXXV X", new int[] {10}); // Bug: should be {11}
        layout("C X", new int[] {1}); // Should be 2
    }

    public void testMultipleSpacesAtBreak() {
        //      0123456789012
        layout("LXX XXXX", new int[] {4});
        layout("LXX  XXXX", new int[] {5});
        layout("LXX   XXXX", new int[] {6});
        layout("LXX    XXXX", new int[] {7});
        layout("LXX     XXXX", new int[] {8});
    }

    public void testZeroWidthCharacters() {
        //      0123456789012345678901234
        layout("X.X.X.X.X.X.X.X.X.X", NO_BREAK);
        layout("...X.X.X.X.X.X.X.X.X.X...", NO_BREAK);
        layout("C.X", new int[] {2});
        layout("C..X", new int[] {3});
    }

    /**
     * Note that when the text has spans, StaticLayout does not use the provided TextPaint to
     * measure text runs anymore. This is probably a bug.
     * To be able to use the fake mTextPaint and make this test pass, use mPaint instead of
     * mWorkPaint in MeasuredText#addStyleRun
     */
    public void testWithSpans() {
        if (!SPAN_TESTS_SUPPORTED) return;

        layout(spanify("<012 456 89>"), NO_BREAK);
        layout(spanify("012 <456> 89"), NO_BREAK);
        layout(spanify("<012> <456>< 89>"), NO_BREAK);
        layout(spanify("<012> <456> <89>"), NO_BREAK);

        layout(spanify("<012> <456> <89>012"), new int[] {8});
        layout(spanify("<012> <456> 89<012>"), new int[] {8});
        layout(spanify("<012> <456> <89><012>"), new int[] {8});
        layout(spanify("<012> <456> 89 <123>"), new int[] {11});
        layout(spanify("<012> <456> 89< 123>"), new int[] {11});
        layout(spanify("<012> <456> <89> <123>"), new int[] {11});
        layout(spanify("012 456 89 <LXX> XX XX"), new int[] {11, 18});
    }

    public void testWithOverlappingSpans() {
        // TODO Also try overlapping spans. The current implementation does not care, but would be
        // good to have before any serious refactoring.
    }

    /*
     * Adding a span to the string should not change the layout, since the metrics are unchanged.
     */
    public void testWithOneSpan() {
        if (!SPAN_TESTS_SUPPORTED) return;

        String[] texts = new String[] { "0123", "012 456", "012 456 89 123", "012 45678 012",
                "012 456 89012 456 89012", "0123456789012" };

        MetricAffectingSpan metricAffectingSpan = getMetricAffectingSpan();

        for (String text : texts) {
            // Get the line breaks without any span
            StaticLayout sl = getStaticLayout(text);
            int[] breaks = getBreaks(sl);

            // Add spans on all possible offsets
            for (int spanStart = 0; spanStart < text.length(); spanStart++) {
                for (int spanEnd = spanStart; spanEnd < text.length(); spanEnd++) {
                    SpannableStringBuilder ssb = new SpannableStringBuilder(text);
                    ssb.setSpan(metricAffectingSpan, spanStart, spanEnd,
                            Spanned.SPAN_INCLUSIVE_INCLUSIVE);
                    layout(ssb, breaks);
                }
            }
        }
    }

    public void testWithTwoSpans() {
        if (!SPAN_TESTS_SUPPORTED) return;

        String[] texts = new String[] { "0123", "012 456", "012 456 89 123", "012 45678 012",
                "012 456 89012 456 89012", "0123456789012" };

        MetricAffectingSpan metricAffectingSpan1 = getMetricAffectingSpan();
        MetricAffectingSpan metricAffectingSpan2 = getMetricAffectingSpan();

        for (String text : texts) {
            // Get the line breaks without any span
            StaticLayout sl = getStaticLayout(text);
            int[] breaks = getBreaks(sl);

            // Add spans on all possible offsets
            for (int spanStart1 = 0; spanStart1 < text.length(); spanStart1++) {
                for (int spanEnd1 = spanStart1; spanEnd1 < text.length(); spanEnd1++) {
                    SpannableStringBuilder ssb = new SpannableStringBuilder(text);
                    ssb.setSpan(metricAffectingSpan1, spanStart1, spanEnd1,
                            Spanned.SPAN_INCLUSIVE_INCLUSIVE);

                    for (int spanStart2 = 0; spanStart2 < text.length(); spanStart2++) {
                        for (int spanEnd2 = spanStart2; spanEnd2 < text.length(); spanEnd2++) {
                            ssb.setSpan(metricAffectingSpan2, spanStart2, spanEnd2,
                                    Spanned.SPAN_INCLUSIVE_INCLUSIVE);
                            layout(ssb, breaks);
                        }
                    }
                }
            }
        }
    }

    public void testBreakCondition() {
        // Try all the different line break characters, space, tab, ','...
    }

    public void testReplacementSpan() {
        // Add ReplacementSpan to the string
    }

    public void testParagraphs() {
        // Add \n to the text
    }

    public void testWithEmoji() {
        // Surrogate emoji characters get replaced by a bitmap
    }

    public void testWithSurrogate() {
        layout("LX" + SURR_FIRST + SURR_SECOND, NO_BREAK);
        layout("LXXXX" + SURR_FIRST + SURR_SECOND, NO_BREAK);
        // LXXXXI (91) + SURR_FIRST (7) fits. But we should not break the surrogate pair
        // Bug: surrogate pair is broken, should be 6 (breaking after the 'V')
        // Maybe not: may be ok if the second character of a pair always has a 0-width
        layout("LXXXXI" + SURR_FIRST + SURR_SECOND, new int[] {7});

        // LXXXXI (95) + SURR_SECOND (3) fits, but this is not a valid surrogate pair, breaking it
        layout("LXXXXV" + SURR_SECOND + SURR_FIRST, new int[] {7});

        layout("C" + SURR_FIRST + SURR_SECOND, new int[] {1});
    }

    public void testNarrowWidth() {
        int[] widths = new int[] { 0, 4, 10 };
        String[] texts = new String[] { "", "X", " ", "XX", "XX ", "X ", "XXX", "XXX ", "X X",
            " X X " /* Bug "X  X", ".X..", "  ", "XX  ", "X." should work too */ };

        for (String text: texts) {
            // 15 is such that only one character will fit
            StaticLayout reference = getStaticLayout(text, 15);
            int[] breaks = getBreaks(reference);

            // Width under 15 should all lead to the same line break
            for (int width: widths) {
                StaticLayout sl = getStaticLayout(text, width);
                layout(sl, text, breaks);
            }
        }
    }
}
