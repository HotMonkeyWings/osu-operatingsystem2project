all: concurrency2

concurrency2: diningPhilosophers.c
	gcc diningPhilosophers.c -lpthread -std=c99 -w -o diningPhilosophers

clean:
	rm -f diningPhilosophers *.pdf *.ps *.dvi *.out *.log *.aux *.bbl *.blg *.pyg

.PHONY: all show clean ps pdf showps
