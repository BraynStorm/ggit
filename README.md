# GGit - Good/Great/GRAPHical/Grotesque GIT

A git GUI that focuses on branches and columns.

## Tasks

- [X] Move the span-collision code (the static vector stuff) to ggit-vector.c/ggit_load_repository
- [ ] Allow compressing into other branches.
- [x] Allow regex matching for special branches.
- [X] Draw lines in __style__.
- [ ] Draw branches on-hover of columns.
- [ ] Draw the HEAD commit in a distinctive way.
- [ ] Draw the refs/stash commits in a distinctive way.
- [X] Make columns start-end-y dependant and compress the graph horizontally when a column is not used.
- [X] Add zoom in/out.
  - [ ] Use the cursor for the "center" of the zoom.
  - [ ] Zoom in the text (font-size) as well.
- [ ] Add configurable branches/colors/order.
- [ ] Add custom filtering (by author, by date, range of commits, etc...).
- [ ] Add clickable GUI
    - [ ] Add "Checkout" - double-click on a head-of-branch commit.
    - [ ] Add "Create branch" - ???
    - [ ] Add "Delete branch"
    - [ ] Add "Rebase" - drag-and-drop a commit.
        - [ ] Holding CTRL does a cherry-pick instead.
            - [ ] Add "Merge"    - right-click-and-drag from the 'kink' to the 'main'.
- [ ] Add auto-reloading.
- [ ] Add support for git tags.
- [ ] Add support for Jenkins.
- [ ] Add support for JIRA.

## Bugs
- [X] Branch bugfix/a is stemmed from bugfix/b but not from the head, rather, somewhere in the middle. But they don't collide
    - [X] ... ??? That makes no sense, yet they are merged.
    - [X] Found it, we should go down first, to the level of the parent and then go left/right. Right now, the graph lines go left-right first and then go down to the parent.
    - [X] Spans are overlapping but merged, why?
- [ ] Feature stemming from another feature extends the span of the parent.
    - [ ] Probably just have to check if the commit has > 1 parent, and only then extend the parent's span.
- [X] Touchpad support is very broken.