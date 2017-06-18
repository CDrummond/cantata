[Desktop Entry]
Type=Application
Name=Cantata
GenericName=Music Player Client
GenericName[bs]=Muzički player klijent
GenericName[da]=Musik afspiller
GenericName[de]=Grafischer Musik Player Client
GenericName[es]=Cliente de reproducción de música
GenericName[fi]=Musiikkisoitinasiakas
GenericName[fr]=Client de lecture de musique
GenericName[gl]=Cliente de reprodución de música
GenericName[hu]=Zenelejátszókliens
GenericName[jv]=Musik Player Client
GenericName[ko]=음악 플레이어 클라이언트
GenericName[ms]=Klien Pemain Musik
GenericName[nb]=Musikkavspiller-klient
GenericName[oc]=Client de lectura de musica
GenericName[pl]=Odtwarzacz muzyki
GenericName[pt]=Reprodutor de música
GenericName[pt_BR]=Reprodutor Multimídia
GenericName[ru]=Клиент музыкального проигрывателя
GenericName[sq]=Clienti player muzike
GenericName[tr]=Muzik Çalıcı İstemcisi
Icon=cantata
Exec=cantata %U
Categories=Qt;KDE;AudioVideo;Player;
X-DBUS-StartupType=Unique
X-DBUS-ServiceName=mpd.cantata
Keywords=Music;MPD;
Actions=Previous;PlayPause;Stop;StopAfterCurrent;Next;

[Desktop Action Previous]
Name=Previous Track
Name[cs]=Předchozí skladba
Name[de]=Vorheriges Stück
Name[es]=Pista anterior
Name[hu]=Előző szám
Name[ko]=이전 곡
Name[pl]=Poprzedni utwór
Name[pt]=Faixa anterior
Name[ru]=Предыдущий трек
Name[zh_CN]=上一个
Exec=@SHARE_INSTALL_PREFIX@/@CMAKE_PROJECT_NAME@/scripts/cantata-remote Previous

[Desktop Action PlayPause]
Name=Play/Pause
Name[cs]=Přehrát/Pozastavit
Name[de]=Abspielen/Pause
Name[es]=Reproducir/Pausa
Name[hu]=Lejátszás/Szünet
Name[ko]=연주/멈춤
Name[pl]=Odtwarzaj/Wstrzymaj
Name[pt]=Reproduzir/pausa
Name[ru]=Воспроизвести/Пауза
Name[zh_CN]=播放/暂停
Exec=@SHARE_INSTALL_PREFIX@/@CMAKE_PROJECT_NAME@/scripts/cantata-remote PlayPause

[Desktop Action Stop]
Name=Stop
Name[cs]=Zastavit
Name[de]=Stopp
Name[es]=Detener
Name[hu]=Állj
Name[ko]=정지
Name[pl]=Stop
Name[pt]=Parar
Name[ru]=Остановить
Name[zh_CN]=停止
Exec=@SHARE_INSTALL_PREFIX@/@CMAKE_PROJECT_NAME@/scripts/cantata-remote Stop

[Desktop Action StopAfterCurrent]
Name=Stop After Current Track
Name[cs]=Zastavit po současné skladbě
Name[de]=Stoppe nach Stück
Name[es]=Detener después de la pista actual
Name[hu]=A mostani szám után leáll
Name[ko]=지금 곡 다음 정지
Name[pl]=Zatrzymaj po obecnym utworze
Name[pt]=Parar após a faixa atual
Name[ru]=Остановить после текущего трека
Name[zh_CN]=当前音轨后停止
Exec=@SHARE_INSTALL_PREFIX@/@CMAKE_PROJECT_NAME@/scripts/cantata-remote StopAfterCurrent

[Desktop Action Next]
Name=Next Track
Name[cs]=Další skladba
Name[de]=Nächstes Stück
Name[es]=Pista siguiente
Name[hu]=Következő szám
Name[ko]=다음 곡
Name[pl]=Następny utwór
Name[pt]=Faixa seguinte
Name[ru]=Следующий трек
Name[zh_CN]=下一个
Exec=@SHARE_INSTALL_PREFIX@/@CMAKE_PROJECT_NAME@/scripts/cantata-remote Next

