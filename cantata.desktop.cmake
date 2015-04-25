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
GenericName[pt]=Um reprodutor de música
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
Actions=Previous;Play;Pause;Stop;StopAfterCurrent;Next;

[Desktop Action Previous]
Name=Previous Track
Name[cs]=Předchozí skladba
Name[de]=Vorheriges Stück
Name[es]=Pista anterior
Name[hu]=Előző szám
Name[ko]=이전 곡
Name[pl]=Poprzedni utwór
Name[ru]=Предыдущий трек
Name[zh_CN]=上一个
Exec=@CMAKE_INSTALL_PREFIX@/share/@CMAKE_PROJECT_NAME@/scripts/cantata-remote Previous
OnlyShowIn=Unity;

[Desktop Action Play]
Name=Play
Name[cs]=Přehrát
Name[de]=Abspielen
Name[es]=Reproducir
Name[hu]=Lejátszás
Name[ko]=연주
Name[pl]=Odtwarzaj
Name[ru]=Воспроизвести
Name[zh_CN]=播放
Exec=@CMAKE_INSTALL_PREFIX@/share/@CMAKE_PROJECT_NAME@/scripts/cantata-remote Play
OnlyShowIn=Unity;

[Desktop Action Pause]
Name=Pause
Name[cs]=Pozastavit
Name[de]=Pause
Name[es]=Pausa
Name[hu]=Szünet
Name[ko]=멈춤
Name[pl]=Wstrzymaj
Name[ru]=Пауза
Name[zh_CN]=暂停
Exec=@CMAKE_INSTALL_PREFIX@/share/@CMAKE_PROJECT_NAME@/scripts/cantata-remote Pause
OnlyShowIn=Unity;

[Desktop Action Stop]
Name=Stop
Name[cs]=Zastavit
Name[de]=Stopp
Name[es]=Detener
Name[hu]=Állj
Name[ko]=정지
Name[pl]=Stop
Name[ru]=Остановить
Name[zh_CN]=停止
Exec=@CMAKE_INSTALL_PREFIX@/share/@CMAKE_PROJECT_NAME@/scripts/cantata-remote Stop
OnlyShowIn=Unity;

[Desktop Action StopAfterCurrent]
Name=Stop After Current Track
Name[cs]=Zastavit po současné skladbě
Name[de]=Stoppe nach Stück
Name[es]=Detener después de la pista actual
Name[hu]=A mostani szám után leáll
Name[ko]=지금 곡 다음 정지
Name[pl]=Zatrzymaj po obecnym utworze
Name[ru]=Остановить после текущего трека
Name[zh_CN]=当前音轨后停止
Exec=@CMAKE_INSTALL_PREFIX@/share/@CMAKE_PROJECT_NAME@/scripts/cantata-remote StopAfterCurrent
OnlyShowIn=Unity;

[Desktop Action Next]
Name=Next Track
Name[cs]=Další skladba
Name[de]=Nächstes Stück
Name[es]=Pista siguiente
Name[hu]=Következő szám
Name[ko]=다음 곡
Name[pl]=Następny utwór
Name[ru]=Следующий трек
Name[zh_CN]=下一个
Exec=@CMAKE_INSTALL_PREFIX@/share/@CMAKE_PROJECT_NAME@/scripts/cantata-remote Next
OnlyShowIn=Unity;

