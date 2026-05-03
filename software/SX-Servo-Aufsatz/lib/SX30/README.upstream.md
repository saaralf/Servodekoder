# SX30 Arduino Selectrix Bibliothek für den ATmega328

Eine Arduino Bibliothek, um Selectrix signale zu dekodieren, zu lesen und zu schreiben.

Diese neue Version 3.0 der bisherigen Bibliothek SX22b (Version 2.2b) wird für das überarbeitete RT-Basisboard V02.00 benötigt. Sie steuert die SX-Write-Leitung des SX-Busses über eine duale Transistor-Endstufe, während die bisherige Version den Controller-Ausgang über einen R100-Widerstand angesteuert hat.

SX30 kann auch für die alten SX-Basisboards (V01.00..V01.02) verwendet werden, wenn in der SX30.h das <#define _SXBASISBOARD_ 1> (statt 2) angegeben wird.

SX30 verwendet ein verbessertes Timing und muss bei der Initialisierung in der setup-Routine mit <i>attachInterrupt(0, sxisr, CHANGE)</i> aufgerufen werden.

Lesen Sie mehr (auch über die benötigte Add-On HW für den Arduino), unter:
<a href="http://opensx.net">opensx.net</a>

usage: 
- SX30-master.zip herunterladen 
- entzippen in den "libraries" folder
- umbennen in "SX30"
- arduino IDE neu starten, Beispiel laden
- HARDWARE bauen !! (siehe opensx.net)
- compilieren und hochladen
- auf der Modellbahn anschliessen und verwenden ...

Die Selectrix Lib wurde entwickelt von Michael Blank und weiterentwickelt von Reinhard Thamm.

(Es gibt verschiedene Weiterentwicklungen zu Selectrix-1, die leider nicht zueinander kompatibel sind, mit denen beschäftigen wir uns hier NICHT.)

Selectrix™ ist ein sehr altes (80ziger Jahre) Modellbahn-Digitalsystem (auch kurz "SX" genannt) und wird vom Hersteller TRIX nicht mehr weiterentwickelt. (das Markenzeichen SelecTRIX gehört TRIX, jetzt Märklin).

Die Originalversion von Selectrix (jetzt Selectrix 1 genannt) ist ziemlich einfach und wurde genormt in den MOROP Normen NEM 680 und NEM 681.


(SX is an open Model Railroad Control System, standardized in
NEM 680 and NEM 681, Selectrix is a Trademark of TRIX - now owned by
Maerklin)

