nothing:
	@echo "This Makefile is really just for Devel::Cover's benefit"
	@echo "however if I ever think of something useful for it to do"
	@echo "it'll start doing that.  Try 'make test'."

test:
	prove --norc --lib --recurse --verbose --shuffle t
