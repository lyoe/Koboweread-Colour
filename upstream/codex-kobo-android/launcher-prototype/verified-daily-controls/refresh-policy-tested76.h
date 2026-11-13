// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_REFRESH_POLICY_H
#define KOBO_REFRESH_POLICY_H
/* Count changed displayed frames, never idle polls. Avoid flashing animations
 * more often than once per 15 seconds. An explicit user refresh bypasses this. */
static int needs_full_refresh(unsigned frames,unsigned partials,double elapsed,int manual){
 return frames==0 || manual || (partials>=12 && elapsed>=15.0);
}
#endif
