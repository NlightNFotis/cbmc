default: tests.log

test:
	@if ! ../test.pl -c ../../../src/cbmc/cbmc ; then \
		../failed-tests-printer.pl ; \
		exit 1 ; \
	fi

tests.log: ../test.pl
	@if ! ../test.pl -c ../../../src/cbmc/cbmc ; then \
		../failed-tests-printer.pl ; \
		exit 1 ; \
	fi

show:
	@for dir in *; do \
		if [ -d "$$dir" ]; then \
			vim -o "$$dir/*.java" "$$dir/*.out"; \
		fi; \
	done;

clean:
	find -name '*.out' -execdir rm -f '{}' \;
	find -name '*.gb' -execdir rm -f '{}' \;
	rm -f tests.log
