#!/bin/sh
# $Id: prerelease.sh,v 1.1 2001/06/19 03:38:58 dwonis Exp $
# wmBiff prerelease script.  Used mainly by the upstream maintainer(s).
# Run this before making a new release of wmBiff
# You will need cvs2cl.

set -e

(
	cd wmbiff
	make indent
	make distclean
	make all
	make distclean
)

cat <<EOF > CHANGES
CVS Changelog.  Don't bother editing this file, because it will get overwritten
by maint/prerelease.sh .

$Log: prerelease.sh,v $
Revision 1.1  2001/06/19 03:38:58  dwonis
* Another big patch that mucks with everything.  I probably deserve to be
  flamed for this practice.  Feel free... :-)
* Added maint/prerelease.sh script.  Run it before making any releases.
* Added maint/changelog.sed.  Add your SourceForge userid here.
* Moved ChangeLog to RELEASE-NOTES (see below).
* Added a new file, CHANGES (created by maint/prerelease.sh) that tabulates
  all the CVS changes.
* Added "distclean" to wmbiff/Makefile.
* Added CVS Id$ to all the files in wmbiff/ .
* I reformatted ths changelog, again.  I hope this is the last time I need
  to do this.  The CVS logs should be used for all changes, and this file
  should by updated for user-visible changes only, from now on.
  (Dwayne C. Litzenberger)
* Updated the README to reflect that Gennady Belyakov died right after releasing
  wmBiff 0.2.  May your soul rest in peace, Gennady.  (Dwayne C. Litzenberger)


EOF

cvs2cl --stdout --utc --day-of-week -t | sed -f maint/changelog.sed >> CHANGES

echo ""
echo ""
echo ""
echo ""
echo "Pre-release script successful!"
echo ""
echo "Don't forget to edit RELEASE-NOTES, and substitute today's date for"
echo "(unreleased), commit and tag the release, then add a new version line"
echo "and commit again."
echo ""
echo "Also, don't forget to update WMBIFF_VERSION in wmbiff/Makefile ."
echo ""
echo "The date for the changelog is: `date -R`"

