default: tests.log

test:
	@../test.pl -c ../../../src/goto-analyzer/goto-analyzer

tests.log: ../test.pl
	@../test.pl -c ../../../src/goto-analyzer/goto-analyzer

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

