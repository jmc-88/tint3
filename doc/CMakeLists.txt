include(Pandoc)

add_pandoc_man_page(
  tint3_1
  INPUT tint3.1.md
  OUTPUT tint3.1
  DESTINATION "${MANDIR}/man1")

add_pandoc_man_page(
  tint3rc_5
  INPUT tint3rc.5.md
  OUTPUT tint3rc.5
  DESTINATION "${MANDIR}/man5")
