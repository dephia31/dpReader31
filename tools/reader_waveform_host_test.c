/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "display_reader_waveform.h"
#include "e0470_epaper_waveform.h"
int main(void){
 e0470_waveform_init();
 const EpdWaveform *wave=display_reader_text_waveform();
 const EpdWaveformPhases *actual=e0470_waveform_phases(wave,MODE_GL16);
 const EpdWaveformPhases *vendor=e0470_waveform_phases(&E0470_WAVEFORM,MODE_GL16);
 assert(actual&&vendor&&actual->phases==37&&vendor->phases==37);
 assert(e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GL16)->phases==48);
 for(int f=0;f<37;++f)for(int from=0;from<16;++from)for(int to=0;to<16;++to)
  assert(e0470_phase_action(actual,f,to,from)==(from==to?0:e0470_phase_action(vendor,f,to,from)));
 puts("PASS: reader preserves every changed vendor GL37 transition in order, holds equal pairs and retains full48 recovery");
}
