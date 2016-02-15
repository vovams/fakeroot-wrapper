CXXFLAGS = -std=gnu++11 -Wall

app = fakeroot-wrapper

.PHONY: all
all: debug

.PHONY: debug
debug: CXXFLAGS += -g
debug: $(app)

.PHONY: release
release: CXXFLAGS += -O3
release: $(app)

.PHONY: release-32
release-32: CXXFLAGS += -m32
release-32: LDFLAGS  += -m32
release-32: release

.PHONY: clean
clean:
	$(RM) $(app)
