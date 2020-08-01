AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = xletters2

xletters2_SOURCES = xletters.c

noinst_HEADERS = xletters.h utf8.h

bin_SCRIPTS = xletters-duel

man_MANS = xletters.6

EXTRA_DIST = xletters.6 xletters-duel xletters.spec.in inputconv.txt jp-en.txt

install-data-hook:
	$(INSTALL) -d -m 755 $(DESTDIR)/var/local/games/lib/xletters
	touch scores
	$(INSTALL) -m 666 scores $(DESTDIR)/var/local/games/lib/xletters/scores_jp-en
	rm -f scores
	$(INSTALL) -d -m 755 $(DESTDIR)/usr/local/share/xletters
	$(INSTALL) -m 666 jp-en.txt $(DESTDIR)/usr/local/share/xletters/jp-en.txt
	$(INSTALL) -m 666 inputconv.txt $(DESTDIR)/usr/local/share/xletters/inputconv.txt
