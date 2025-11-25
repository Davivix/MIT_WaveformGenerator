# MIT_WaveformGenerator

Jednoduchý kód pro generování průběhů pomocí dvou DAC převodníků. Hodnocený známkou 2

Kód je v aktuálním stavu funkčí, avšak není bez chyb / bugů:

  bug 1#: Z nějakého důvodu je nastavování frekvence strašně nepřesný. Všiml jsem si toho, když jsem nastavil 1Hz, a doba periody byla přibližně 2,5s ???

  chyba 1#: Místo toho, aby se pokaždý v hlavní smyčce počítala výstupní hodnota tímto způsobem "ch1_out = (Waveforms[x][y] * ch1_amplituda) / 255", tak by se při změně amplitudy mělo vypočítat nový pole hodnot, aby se nemuselo dělit (náročná operace na CPU time)
  chyba 2#: Parametrizace, v konzoli se nastavuje amplituda jako hodnota 0-255. To je špatně, musí to být 0-5V. V kódu by se ta hodnota následně převedla na požadovanou hodnotu 0-255

  připomínka 1#: Použití přerušení by tuhle úlohu strašně ulehčilo. Protože není, tak v hlavní smyčce je dohromady vzorkování a výpočty s doubly, což dělá to časování nepřesný. Například když jsou oba kanály nastavený na stejnou frekvenci, tak je mezi nimi lehký offset.
