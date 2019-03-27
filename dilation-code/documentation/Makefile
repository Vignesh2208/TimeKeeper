document = TimeKeeper_Documentation
SOURCES = $(wildcard *.tex) 

all: $(document).pdf

pdf : $(document).pdf

$(document).pdf : $(SOURCES)
	latex $(document)
	latex $(document)
	latex $(document)
	dvips $(document).dvi
	ps2pdf $(document).ps
	rm -f *.aux *.bbl *.log *.blg *.lot *.lof *.out *.ps *.dvi *.toc

clean:
	rm -f *.aux *.bbl *.log *.blg *.lot *.lof *.out *.ps *.dvi *.toc $(document).pdf
