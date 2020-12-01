with import <nixpkgs> {};

let
  ocamlPackages = pkgs.recurseIntoAttrs pkgs.ocamlPackages;
  ocamlVersion = (builtins.parseDrvName ocamlPackages.ocaml.name).version;
  findlibSiteLib = "${ocamlPackages.findlib}/lib/ocaml/${ocamlVersion}/site-lib";
  ocamlInit = pkgs.writeText "ocamlinit" ''
    let () =
      try Topdirs.dir_directory "${findlibSiteLib}"
      with Not_found -> ()
    ;;

    #use "topfind";;
    #thread;;
    #camlp4o;;
    #require "core";;
    #require "core.syntax";;
  '';
in
pkgs.mkShell rec {
  buildInputs = with pkgs; [
    dune
  ] ++ ( with ocamlPackages;
  [
    ocaml
    core
    core_extended
    findlib
    utop
    merlin
    ocp-indent
  ]);
  IN_NIX_SHELL = 1;
  UTOP_SITE_LISP = "${ocamlPackages.utop}/share/emacs/site-lisp";
  MERLIN_SITE_LISP = "${ocamlPackages.merlin}/share/emacs/site-lisp";
  OCP_INDENT_SITE_LISP="${ocamlPackages.ocp-indent}/share/emacs/site-lisp";
  OCAMLINIT = "${ocamlInit}";
  shellHook = ''
    alias utop="utop -init ${ocamlInit}"
    alias ocaml="ocaml -init ${ocamlInit}"
    emacs &
  '';
}
