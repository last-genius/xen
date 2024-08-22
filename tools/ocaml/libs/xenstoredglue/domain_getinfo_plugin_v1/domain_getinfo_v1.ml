(** Minimal interface on top of unstable Xenctrl for Oxenstored's usage *)

(** For the full Xenctrl interface, see: tools/ocaml/libs/xc/ *)

module M : Plugin_interface_v1.Domain_getinfo_V1 = struct
  exception Error of string

  type domid = int
  type handle

  type domaininfo = {
    domid : domid;
    dying : bool;
    shutdown : bool;
    shutdown_code : int;
  }

  external interface_open : unit -> handle = "stub_xsglue_xc_interface_open"

  external domain_getinfo : handle -> domid -> domaininfo
    = "stub_xsglue_xc_domain_getinfo"

  external __domain_getinfolist : handle -> domid -> int -> domaininfo list
    = "stub_xsglue_xc_domain_getinfolist"

  let domain_getinfolist handle first_domain =
    (* [rev_concat lst] is equivalent to [lst |> List.concat |> List.rev]
     * except it is tail recursive, whereas [List.concat] isn't.
     * Example:
     * rev_concat [[10;9;8];[7;6];[5]]] = [5; 6; 7; 8; 9; 10]
     *)
    let rev_append_fold acc e = List.rev_append e acc in
    let rev_concat lst = List.fold_left rev_append_fold [] lst in

    let nb = 1024 in
    let rec __getlist lst from =
      (* _domain_getinfolist returns domains in reverse order, largest first *)
      match __domain_getinfolist handle from nb with
      | [] -> rev_concat lst
      | hd :: _ as l -> __getlist (l :: lst) (hd.domid + 1)
    in
    __getlist [] first_domain

  let _ = Callback.register_exception "xsg.error" (Error "register_callback")
end

let () =
  Printf.printf "Registration of %s plugin started\n%!" __MODULE__;
  Plugin_interface_v1.register_plugin_v1
    (module M : Plugin_interface_v1.Domain_getinfo_V1);
  Printf.printf "Registration of %s plugin successful\n%!" __MODULE__
