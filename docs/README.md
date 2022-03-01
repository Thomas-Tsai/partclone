# How to contribute

You can contribute to the documentation in several ways:
* Open an issue
* If you are partclone core developer you know what to do
* Other developers can fork the repository, modify files and make a pull request, see example below

For anything more complex than fixing a typo it is best to first discuss the proposed changes in an issue.

Example (fixing a typo):

Fork the repository on the github website (requires a github user account).

Then (replace `username` with your username):
```
git clone git@github.com:username/partclone.git
cd partclone/
```

Ensure we are on the main branch and create a new branch for our issue:
```
git branch
git checkout master
git checkout -b docs-typo-normanly
```

Change files:
```
cd docs/
fgrep -i  normanly *.xml
vi partclone.xml
vi partclone.chkimg.xml
vi partclone.dd.xml
vi partclone.imager.xml
vi partclone.restore.xml
```

Create first commit, referencing the issue:
(TODO: Is issue #123 enough or should the full URL be used since the fork doesn't have any issues?)
```
git diff
git status
git add partclone.xml partclone.chkimg.xml partclone.dd.xml partclone.imager.xml partclone.restore.xml 
git commit
git push --set-upstream origin docs-typo-normanly
```

Derive `.8` files.
You may have to install `libxslt-tools`, `docbook` and `docbook-xsl-stylesheets`.
(TODO: What minimum version is required?)
(TODO: Is `xmltoman` or `doxygen2man` needed?)
The file `Makefile.am` configures a specific stylesheet to be used in the conversion that
may not be on your installation. Try a close match before manually installing the exact
version of the `partclone` developers, e.g.:
```
xsltproc --nonet /usr/share/xml/docbook/stylesheet/nwalsh/1.79.2/manpages/docbook.xsl partclone.xml
for I in chkimg dd imager restore ; do xsltproc --nonet /usr/share/xml/docbook/stylesheet/nwalsh/1.79.2/manpages/docbook.xsl partclone.$I.xml ; done
```

Check there are no style changes or other changes than intended:
```
git diff
```

Create second commit:
```
git status
git add partclone.restore.8 partclone.imager.8 partclone.dd.8 partclone.chkimg.8 partclone.8
git commit
git push
```

Visit your fork on github. There is a button to make a pull request. If too much time passed the button may no longer be on the main page of your fork but you
may find such a button when switching to the branch.
