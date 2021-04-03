{
  description = "mangekyou";

  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils, ... }
  : flake-utils.lib.eachSystem [ "x86_64-linux" ]
    (system:
      let pkgs = import nixpkgs { inherit system; };
      in {
        devShell = pkgs.mkShell.override
          { stdenv = pkgs.llvmPackages_11.stdenv;}
          {
            buildInputs = with pkgs; [ cmake ninja ];
          };
      }
    );
}

