/*
  File autogenerated by gengetopt version 2.23
  generated with the following command:
  gengetopt -i cmdoptions.ggo 

  The developers of gengetopt consider the fixed text that goes in all
  gengetopt output files to be in the public domain:
  we make no copyright claims on it.
*/

/* If we use autoconf.  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FIX_UNUSED
#define FIX_UNUSED(X) (void) (X) /* avoid warnings for unused params */
#endif

#include <getopt.h>

#include "cmdline.h"

const char *gengetopt_args_info_purpose = "The reference implementation node of the Warthog Network.\n\n\nBy Pumbaa, Timon & Rafiki";

const char *gengetopt_args_info_usage = "Usage: wart-node [OPTION]...";

const char *gengetopt_args_info_versiontext = "";

const char *gengetopt_args_info_description = "";

const char *gengetopt_args_info_detailed_help[] = {
  "  -h, --help                 Print help and exit",
  "      --detailed-help        Print help, including all details and hidden\n                               options, and exit",
  "  -V, --version              Print version and exit",
  "\nNode options:",
  "  -b, --bind=IP:PORT         Socket to listen on, defaults to \"0.0.0.0:9186\"\n                               for main net and \"0.0.0.0:9286\" for test net",
  "  -C, --connect=IP:PORT,...  Specify peer list",
  "  This option overrides the peers list, specify comma separated entries of\n  format 'IP:PORT'",
  "      --isolated             Do not allow peers (for testing purposes, do not\n                               use in production)",
  "  This option isolates the node such that it does not connect to other peers\n  and does not accept incoming connections from other peers. This option is for\n  debugging and testing purposes only, do not use in production, mined blocks\n  will not be included in main net",
  "      --temporary            Use temporary database (for testing purposes, do\n                               not use in production)",
  "  This option starts the node with a temporary empty chain database.",
  "      --testnet              Enable testnet",
  "      --disable-tx-mining    Don't mine transactions (in case of bugs)",
  "\nData file options:",
  "      --chain-db=STRING      specify chain data file",
  "  Defaults to ~/.warthog/chain.db3 in Linux, %LOCALAPPDATA%/Warthog/chain.db3\n  on Windows.",
  "      --peers-db=STRING      specify data file",
  "  Defaults to ~/.warthog/peers.db3 in Linux, %LOCALAPPDATA%/Warthog/peers.db3\n  on Windows",
  "\nWebsocket server options:",
  "      --ws-port=INT          Websocket port",
  "  Public websocket nodes support communication over websocket",
  "      --ws-tls-cert=STRING   TLS certificate file for public websocket endpoint",
  "  Defaults to 'ws.cert'",
  "      --ws-tls-key=STRING    TLS private key file for public websocket endpoint",
  "  Defaults to 'ws.key'",
  "      --ws-x-forwarded-for   Honor 'X-Forwarded-For' header to determine peer\n                               IP. Intended use for reverse-proxies.",
  "  By default the node uses the connection's peer IP for limits and bans.\n  However, when behind a reverse proxy the real peer's IP must be determined\n  using 'X-Forwarded-For' header.",
  "      --ws-bind-localhost    Bind to loopback interface. Intended use for\n                               reverse-proxies.",
  "  Only websocket connections from localhost are accepted. This is to hide the\n  node when websocket TLS encryption is done by a reverse proxy like nginx.\n  This should not be used when the node is started within a docker container\n  with forwarded ports.",
  "\nLogging options:",
  "  -d, --debug                Enable debug messages",
  "\nJSON RPC endpoint options:",
  "  -r, --rpc=IP:PORT          JSON RPC endpoint socket, defaults to\n                               \"127.0.0.1:3000\" for main net and\n                               \"127.0.0.1:3100\" for test net",
  "      --publicrpc=IP:PORT    Public JSON RPC endpoint socket, disabled by\n                               default",
  "      --stratum=IP:PORT      Solo mining stratum",
  "      --enable-public        Shorthand for --publicrpc=0.0.0.0:3001",
  "\nConfiguration file options:",
  "  -c, --config=FILENAME      Configuration file, default is \"config.toml\", in\n                               testnet \"testnet3_chain.db3\"",
  "  -t, --test                 test the configuration file (check for correct\n                               syntax)",
  "      --dump-config          dump the current configuration to stdout",
  "  This option can be useful to generate a configuration file template",
    0
};

static void
init_help_array(void)
{
  gengetopt_args_info_help[0] = gengetopt_args_info_detailed_help[0];
  gengetopt_args_info_help[1] = gengetopt_args_info_detailed_help[1];
  gengetopt_args_info_help[2] = gengetopt_args_info_detailed_help[2];
  gengetopt_args_info_help[3] = gengetopt_args_info_detailed_help[3];
  gengetopt_args_info_help[4] = gengetopt_args_info_detailed_help[4];
  gengetopt_args_info_help[5] = gengetopt_args_info_detailed_help[5];
  gengetopt_args_info_help[6] = gengetopt_args_info_detailed_help[7];
  gengetopt_args_info_help[7] = gengetopt_args_info_detailed_help[9];
  gengetopt_args_info_help[8] = gengetopt_args_info_detailed_help[11];
  gengetopt_args_info_help[9] = gengetopt_args_info_detailed_help[12];
  gengetopt_args_info_help[10] = gengetopt_args_info_detailed_help[13];
  gengetopt_args_info_help[11] = gengetopt_args_info_detailed_help[14];
  gengetopt_args_info_help[12] = gengetopt_args_info_detailed_help[16];
  gengetopt_args_info_help[13] = gengetopt_args_info_detailed_help[18];
  gengetopt_args_info_help[14] = gengetopt_args_info_detailed_help[19];
  gengetopt_args_info_help[15] = gengetopt_args_info_detailed_help[21];
  gengetopt_args_info_help[16] = gengetopt_args_info_detailed_help[23];
  gengetopt_args_info_help[17] = gengetopt_args_info_detailed_help[25];
  gengetopt_args_info_help[18] = gengetopt_args_info_detailed_help[27];
  gengetopt_args_info_help[19] = gengetopt_args_info_detailed_help[29];
  gengetopt_args_info_help[20] = gengetopt_args_info_detailed_help[30];
  gengetopt_args_info_help[21] = gengetopt_args_info_detailed_help[31];
  gengetopt_args_info_help[22] = gengetopt_args_info_detailed_help[32];
  gengetopt_args_info_help[23] = gengetopt_args_info_detailed_help[33];
  gengetopt_args_info_help[24] = gengetopt_args_info_detailed_help[34];
  gengetopt_args_info_help[25] = gengetopt_args_info_detailed_help[35];
  gengetopt_args_info_help[26] = gengetopt_args_info_detailed_help[36];
  gengetopt_args_info_help[27] = gengetopt_args_info_detailed_help[37];
  gengetopt_args_info_help[28] = gengetopt_args_info_detailed_help[38];
  gengetopt_args_info_help[29] = gengetopt_args_info_detailed_help[39];
  gengetopt_args_info_help[30] = 0; 
  
}

const char *gengetopt_args_info_help[31];

typedef enum {ARG_NO
  , ARG_STRING
  , ARG_INT
} cmdline_parser_arg_type;

static
void clear_given (struct gengetopt_args_info *args_info);
static
void clear_args (struct gengetopt_args_info *args_info);

static int
cmdline_parser_internal (int argc, char **argv, struct gengetopt_args_info *args_info,
                        struct cmdline_parser_params *params, const char *additional_error);


static char *
gengetopt_strdup (const char *s);

static
void clear_given (struct gengetopt_args_info *args_info)
{
  args_info->help_given = 0 ;
  args_info->detailed_help_given = 0 ;
  args_info->version_given = 0 ;
  args_info->bind_given = 0 ;
  args_info->connect_given = 0 ;
  args_info->isolated_given = 0 ;
  args_info->temporary_given = 0 ;
  args_info->testnet_given = 0 ;
  args_info->disable_tx_mining_given = 0 ;
  args_info->chain_db_given = 0 ;
  args_info->peers_db_given = 0 ;
  args_info->ws_port_given = 0 ;
  args_info->ws_tls_cert_given = 0 ;
  args_info->ws_tls_key_given = 0 ;
  args_info->ws_x_forwarded_for_given = 0 ;
  args_info->ws_bind_localhost_given = 0 ;
  args_info->debug_given = 0 ;
  args_info->rpc_given = 0 ;
  args_info->publicrpc_given = 0 ;
  args_info->stratum_given = 0 ;
  args_info->enable_public_given = 0 ;
  args_info->config_given = 0 ;
  args_info->test_given = 0 ;
  args_info->dump_config_given = 0 ;
}

static
void clear_args (struct gengetopt_args_info *args_info)
{
  FIX_UNUSED (args_info);
  args_info->bind_arg = NULL;
  args_info->bind_orig = NULL;
  args_info->connect_arg = NULL;
  args_info->connect_orig = NULL;
  args_info->chain_db_arg = NULL;
  args_info->chain_db_orig = NULL;
  args_info->peers_db_arg = NULL;
  args_info->peers_db_orig = NULL;
  args_info->ws_port_orig = NULL;
  args_info->ws_tls_cert_arg = NULL;
  args_info->ws_tls_cert_orig = NULL;
  args_info->ws_tls_key_arg = NULL;
  args_info->ws_tls_key_orig = NULL;
  args_info->rpc_arg = NULL;
  args_info->rpc_orig = NULL;
  args_info->publicrpc_arg = NULL;
  args_info->publicrpc_orig = NULL;
  args_info->stratum_arg = NULL;
  args_info->stratum_orig = NULL;
  args_info->config_arg = NULL;
  args_info->config_orig = NULL;
  
}

static
void init_args_info(struct gengetopt_args_info *args_info)
{

  init_help_array(); 
  args_info->help_help = gengetopt_args_info_detailed_help[0] ;
  args_info->detailed_help_help = gengetopt_args_info_detailed_help[1] ;
  args_info->version_help = gengetopt_args_info_detailed_help[2] ;
  args_info->bind_help = gengetopt_args_info_detailed_help[4] ;
  args_info->connect_help = gengetopt_args_info_detailed_help[5] ;
  args_info->isolated_help = gengetopt_args_info_detailed_help[7] ;
  args_info->temporary_help = gengetopt_args_info_detailed_help[9] ;
  args_info->testnet_help = gengetopt_args_info_detailed_help[11] ;
  args_info->disable_tx_mining_help = gengetopt_args_info_detailed_help[12] ;
  args_info->chain_db_help = gengetopt_args_info_detailed_help[14] ;
  args_info->peers_db_help = gengetopt_args_info_detailed_help[16] ;
  args_info->ws_port_help = gengetopt_args_info_detailed_help[19] ;
  args_info->ws_tls_cert_help = gengetopt_args_info_detailed_help[21] ;
  args_info->ws_tls_key_help = gengetopt_args_info_detailed_help[23] ;
  args_info->ws_x_forwarded_for_help = gengetopt_args_info_detailed_help[25] ;
  args_info->ws_bind_localhost_help = gengetopt_args_info_detailed_help[27] ;
  args_info->debug_help = gengetopt_args_info_detailed_help[30] ;
  args_info->rpc_help = gengetopt_args_info_detailed_help[32] ;
  args_info->publicrpc_help = gengetopt_args_info_detailed_help[33] ;
  args_info->stratum_help = gengetopt_args_info_detailed_help[34] ;
  args_info->enable_public_help = gengetopt_args_info_detailed_help[35] ;
  args_info->config_help = gengetopt_args_info_detailed_help[37] ;
  args_info->test_help = gengetopt_args_info_detailed_help[38] ;
  args_info->dump_config_help = gengetopt_args_info_detailed_help[39] ;
  
}

void
cmdline_parser_print_version (void)
{
  printf ("%s %s\n",
     (strlen(CMDLINE_PARSER_PACKAGE_NAME) ? CMDLINE_PARSER_PACKAGE_NAME : CMDLINE_PARSER_PACKAGE),
     CMDLINE_PARSER_VERSION);

  if (strlen(gengetopt_args_info_versiontext) > 0)
    printf("\n%s\n", gengetopt_args_info_versiontext);
}

static void print_help_common(void)
{
	size_t len_purpose = strlen(gengetopt_args_info_purpose);
	size_t len_usage = strlen(gengetopt_args_info_usage);

	if (len_usage > 0) {
		printf("%s\n", gengetopt_args_info_usage);
	}
	if (len_purpose > 0) {
		printf("%s\n", gengetopt_args_info_purpose);
	}

	if (len_usage || len_purpose) {
		printf("\n");
	}

	if (strlen(gengetopt_args_info_description) > 0) {
		printf("%s\n\n", gengetopt_args_info_description);
	}
}

void
cmdline_parser_print_help (void)
{
  int i = 0;
  print_help_common();
  while (gengetopt_args_info_help[i])
    printf("%s\n", gengetopt_args_info_help[i++]);
}

void
cmdline_parser_print_detailed_help (void)
{
  int i = 0;
  print_help_common();
  while (gengetopt_args_info_detailed_help[i])
    printf("%s\n", gengetopt_args_info_detailed_help[i++]);
}

void
cmdline_parser_init (struct gengetopt_args_info *args_info)
{
  clear_given (args_info);
  clear_args (args_info);
  init_args_info (args_info);
}

void
cmdline_parser_params_init(struct cmdline_parser_params *params)
{
  if (params)
    { 
      params->override = 0;
      params->initialize = 1;
      params->check_required = 1;
      params->check_ambiguity = 0;
      params->print_errors = 1;
    }
}

struct cmdline_parser_params *
cmdline_parser_params_create(void)
{
  struct cmdline_parser_params *params = 
    (struct cmdline_parser_params *)malloc(sizeof(struct cmdline_parser_params));
  cmdline_parser_params_init(params);  
  return params;
}

static void
free_string_field (char **s)
{
  if (*s)
    {
      free (*s);
      *s = 0;
    }
}


static void
cmdline_parser_release (struct gengetopt_args_info *args_info)
{

  free_string_field (&(args_info->bind_arg));
  free_string_field (&(args_info->bind_orig));
  free_string_field (&(args_info->connect_arg));
  free_string_field (&(args_info->connect_orig));
  free_string_field (&(args_info->chain_db_arg));
  free_string_field (&(args_info->chain_db_orig));
  free_string_field (&(args_info->peers_db_arg));
  free_string_field (&(args_info->peers_db_orig));
  free_string_field (&(args_info->ws_port_orig));
  free_string_field (&(args_info->ws_tls_cert_arg));
  free_string_field (&(args_info->ws_tls_cert_orig));
  free_string_field (&(args_info->ws_tls_key_arg));
  free_string_field (&(args_info->ws_tls_key_orig));
  free_string_field (&(args_info->rpc_arg));
  free_string_field (&(args_info->rpc_orig));
  free_string_field (&(args_info->publicrpc_arg));
  free_string_field (&(args_info->publicrpc_orig));
  free_string_field (&(args_info->stratum_arg));
  free_string_field (&(args_info->stratum_orig));
  free_string_field (&(args_info->config_arg));
  free_string_field (&(args_info->config_orig));
  
  

  clear_given (args_info);
}


static void
write_into_file(FILE *outfile, const char *opt, const char *arg, const char *values[])
{
  FIX_UNUSED (values);
  if (arg) {
    fprintf(outfile, "%s=\"%s\"\n", opt, arg);
  } else {
    fprintf(outfile, "%s\n", opt);
  }
}


int
cmdline_parser_dump(FILE *outfile, struct gengetopt_args_info *args_info)
{
  int i = 0;

  if (!outfile)
    {
      fprintf (stderr, "%s: cannot dump options to stream\n", CMDLINE_PARSER_PACKAGE);
      return EXIT_FAILURE;
    }

  if (args_info->help_given)
    write_into_file(outfile, "help", 0, 0 );
  if (args_info->detailed_help_given)
    write_into_file(outfile, "detailed-help", 0, 0 );
  if (args_info->version_given)
    write_into_file(outfile, "version", 0, 0 );
  if (args_info->bind_given)
    write_into_file(outfile, "bind", args_info->bind_orig, 0);
  if (args_info->connect_given)
    write_into_file(outfile, "connect", args_info->connect_orig, 0);
  if (args_info->isolated_given)
    write_into_file(outfile, "isolated", 0, 0 );
  if (args_info->temporary_given)
    write_into_file(outfile, "temporary", 0, 0 );
  if (args_info->testnet_given)
    write_into_file(outfile, "testnet", 0, 0 );
  if (args_info->disable_tx_mining_given)
    write_into_file(outfile, "disable-tx-mining", 0, 0 );
  if (args_info->chain_db_given)
    write_into_file(outfile, "chain-db", args_info->chain_db_orig, 0);
  if (args_info->peers_db_given)
    write_into_file(outfile, "peers-db", args_info->peers_db_orig, 0);
  if (args_info->ws_port_given)
    write_into_file(outfile, "ws-port", args_info->ws_port_orig, 0);
  if (args_info->ws_tls_cert_given)
    write_into_file(outfile, "ws-tls-cert", args_info->ws_tls_cert_orig, 0);
  if (args_info->ws_tls_key_given)
    write_into_file(outfile, "ws-tls-key", args_info->ws_tls_key_orig, 0);
  if (args_info->ws_x_forwarded_for_given)
    write_into_file(outfile, "ws-x-forwarded-for", 0, 0 );
  if (args_info->ws_bind_localhost_given)
    write_into_file(outfile, "ws-bind-localhost", 0, 0 );
  if (args_info->debug_given)
    write_into_file(outfile, "debug", 0, 0 );
  if (args_info->rpc_given)
    write_into_file(outfile, "rpc", args_info->rpc_orig, 0);
  if (args_info->publicrpc_given)
    write_into_file(outfile, "publicrpc", args_info->publicrpc_orig, 0);
  if (args_info->stratum_given)
    write_into_file(outfile, "stratum", args_info->stratum_orig, 0);
  if (args_info->enable_public_given)
    write_into_file(outfile, "enable-public", 0, 0 );
  if (args_info->config_given)
    write_into_file(outfile, "config", args_info->config_orig, 0);
  if (args_info->test_given)
    write_into_file(outfile, "test", 0, 0 );
  if (args_info->dump_config_given)
    write_into_file(outfile, "dump-config", 0, 0 );
  

  i = EXIT_SUCCESS;
  return i;
}

int
cmdline_parser_file_save(const char *filename, struct gengetopt_args_info *args_info)
{
  FILE *outfile;
  int i = 0;

  outfile = fopen(filename, "w");

  if (!outfile)
    {
      fprintf (stderr, "%s: cannot open file for writing: %s\n", CMDLINE_PARSER_PACKAGE, filename);
      return EXIT_FAILURE;
    }

  i = cmdline_parser_dump(outfile, args_info);
  fclose (outfile);

  return i;
}

void
cmdline_parser_free (struct gengetopt_args_info *args_info)
{
  cmdline_parser_release (args_info);
}

/** @brief replacement of strdup, which is not standard */
char *
gengetopt_strdup (const char *s)
{
  char *result = 0;
  if (!s)
    return result;

  result = (char*)malloc(strlen(s) + 1);
  if (result == (char*)0)
    return (char*)0;
  strcpy(result, s);
  return result;
}

int
cmdline_parser (int argc, char **argv, struct gengetopt_args_info *args_info)
{
  return cmdline_parser2 (argc, argv, args_info, 0, 1, 1);
}

int
cmdline_parser_ext (int argc, char **argv, struct gengetopt_args_info *args_info,
                   struct cmdline_parser_params *params)
{
  int result;
  result = cmdline_parser_internal (argc, argv, args_info, params, 0);

  if (result == EXIT_FAILURE)
    {
      cmdline_parser_free (args_info);
      exit (EXIT_FAILURE);
    }
  
  return result;
}

int
cmdline_parser2 (int argc, char **argv, struct gengetopt_args_info *args_info, int override, int initialize, int check_required)
{
  int result;
  struct cmdline_parser_params params;
  
  params.override = override;
  params.initialize = initialize;
  params.check_required = check_required;
  params.check_ambiguity = 0;
  params.print_errors = 1;

  result = cmdline_parser_internal (argc, argv, args_info, &params, 0);

  if (result == EXIT_FAILURE)
    {
      cmdline_parser_free (args_info);
      exit (EXIT_FAILURE);
    }
  
  return result;
}

int
cmdline_parser_required (struct gengetopt_args_info *args_info, const char *prog_name)
{
  FIX_UNUSED (args_info);
  FIX_UNUSED (prog_name);
  return EXIT_SUCCESS;
}


static char *package_name = 0;

/**
 * @brief updates an option
 * @param field the generic pointer to the field to update
 * @param orig_field the pointer to the orig field
 * @param field_given the pointer to the number of occurrence of this option
 * @param prev_given the pointer to the number of occurrence already seen
 * @param value the argument for this option (if null no arg was specified)
 * @param possible_values the possible values for this option (if specified)
 * @param default_value the default value (in case the option only accepts fixed values)
 * @param arg_type the type of this option
 * @param check_ambiguity @see cmdline_parser_params.check_ambiguity
 * @param override @see cmdline_parser_params.override
 * @param no_free whether to free a possible previous value
 * @param multiple_option whether this is a multiple option
 * @param long_opt the corresponding long option
 * @param short_opt the corresponding short option (or '-' if none)
 * @param additional_error possible further error specification
 */
static
int update_arg(void *field, char **orig_field,
               unsigned int *field_given, unsigned int *prev_given, 
               char *value, const char *possible_values[],
               const char *default_value,
               cmdline_parser_arg_type arg_type,
               int check_ambiguity, int override,
               int no_free, int multiple_option,
               const char *long_opt, char short_opt,
               const char *additional_error)
{
  char *stop_char = 0;
  const char *val = value;
  int found;
  char **string_field;
  FIX_UNUSED (field);

  stop_char = 0;
  found = 0;

  if (!multiple_option && prev_given && (*prev_given || (check_ambiguity && *field_given)))
    {
      if (short_opt != '-')
        fprintf (stderr, "%s: `--%s' (`-%c') option given more than once%s\n", 
               package_name, long_opt, short_opt,
               (additional_error ? additional_error : ""));
      else
        fprintf (stderr, "%s: `--%s' option given more than once%s\n", 
               package_name, long_opt,
               (additional_error ? additional_error : ""));
      return 1; /* failure */
    }

  FIX_UNUSED (default_value);
    
  if (field_given && *field_given && ! override)
    return 0;
  if (prev_given)
    (*prev_given)++;
  if (field_given)
    (*field_given)++;
  if (possible_values)
    val = possible_values[found];

  switch(arg_type) {
  case ARG_INT:
    if (val) *((int *)field) = strtol (val, &stop_char, 0);
    break;
  case ARG_STRING:
    if (val) {
      string_field = (char **)field;
      if (!no_free && *string_field)
        free (*string_field); /* free previous string */
      *string_field = gengetopt_strdup (val);
    }
    break;
  default:
    break;
  };

  /* check numeric conversion */
  switch(arg_type) {
  case ARG_INT:
    if (val && !(stop_char && *stop_char == '\0')) {
      fprintf(stderr, "%s: invalid numeric value: %s\n", package_name, val);
      return 1; /* failure */
    }
    break;
  default:
    ;
  };

  /* store the original value */
  switch(arg_type) {
  case ARG_NO:
    break;
  default:
    if (value && orig_field) {
      if (no_free) {
        *orig_field = value;
      } else {
        if (*orig_field)
          free (*orig_field); /* free previous string */
        *orig_field = gengetopt_strdup (value);
      }
    }
  };

  return 0; /* OK */
}


int
cmdline_parser_internal (
  int argc, char **argv, struct gengetopt_args_info *args_info,
                        struct cmdline_parser_params *params, const char *additional_error)
{
  int c;	/* Character of the parsed option.  */

  int error_occurred = 0;
  struct gengetopt_args_info local_args_info;
  
  int override;
  int initialize;
  int check_required;
  int check_ambiguity;
  
  package_name = argv[0];
  
  /* TODO: Why is this here? It is not used anywhere. */
  override = params->override;
  FIX_UNUSED(override);

  initialize = params->initialize;
  check_required = params->check_required;

  /* TODO: Why is this here? It is not used anywhere. */
  check_ambiguity = params->check_ambiguity;
  FIX_UNUSED(check_ambiguity);

  if (initialize)
    cmdline_parser_init (args_info);

  cmdline_parser_init (&local_args_info);

  optarg = 0;
  optind = 0;
  opterr = params->print_errors;
  optopt = '?';

  while (1)
    {
      int option_index = 0;

      static struct option long_options[] = {
        { "help",	0, NULL, 'h' },
        { "detailed-help",	0, NULL, 0 },
        { "version",	0, NULL, 'V' },
        { "bind",	1, NULL, 'b' },
        { "connect",	1, NULL, 'C' },
        { "isolated",	0, NULL, 0 },
        { "temporary",	0, NULL, 0 },
        { "testnet",	0, NULL, 0 },
        { "disable-tx-mining",	0, NULL, 0 },
        { "chain-db",	1, NULL, 0 },
        { "peers-db",	1, NULL, 0 },
        { "ws-port",	1, NULL, 0 },
        { "ws-tls-cert",	1, NULL, 0 },
        { "ws-tls-key",	1, NULL, 0 },
        { "ws-x-forwarded-for",	0, NULL, 0 },
        { "ws-bind-localhost",	0, NULL, 0 },
        { "debug",	0, NULL, 'd' },
        { "rpc",	1, NULL, 'r' },
        { "publicrpc",	1, NULL, 0 },
        { "stratum",	1, NULL, 0 },
        { "enable-public",	0, NULL, 0 },
        { "config",	1, NULL, 'c' },
        { "test",	0, NULL, 't' },
        { "dump-config",	0, NULL, 0 },
        { 0,  0, 0, 0 }
      };

      c = getopt_long (argc, argv, "hVb:C:dr:c:t", long_options, &option_index);

      if (c == -1) break;	/* Exit from `while (1)' loop.  */

      switch (c)
        {
        case 'h':	/* Print help and exit.  */
          cmdline_parser_print_help ();
          cmdline_parser_free (&local_args_info);
          exit (EXIT_SUCCESS);

        case 'V':	/* Print version and exit.  */
          cmdline_parser_print_version ();
          cmdline_parser_free (&local_args_info);
          exit (EXIT_SUCCESS);

        case 'b':	/* Socket to listen on, defaults to \"0.0.0.0:9186\" for main net and \"0.0.0.0:9286\" for test net.  */
        
        
          if (update_arg( (void *)&(args_info->bind_arg), 
               &(args_info->bind_orig), &(args_info->bind_given),
              &(local_args_info.bind_given), optarg, 0, 0, ARG_STRING,
              check_ambiguity, override, 0, 0,
              "bind", 'b',
              additional_error))
            goto failure;
        
          break;
        case 'C':	/* Specify peer list.  */
        
        
          if (update_arg( (void *)&(args_info->connect_arg), 
               &(args_info->connect_orig), &(args_info->connect_given),
              &(local_args_info.connect_given), optarg, 0, 0, ARG_STRING,
              check_ambiguity, override, 0, 0,
              "connect", 'C',
              additional_error))
            goto failure;
        
          break;
        case 'd':	/* Enable debug messages.  */
        
        
          if (update_arg( 0 , 
               0 , &(args_info->debug_given),
              &(local_args_info.debug_given), optarg, 0, 0, ARG_NO,
              check_ambiguity, override, 0, 0,
              "debug", 'd',
              additional_error))
            goto failure;
        
          break;
        case 'r':	/* JSON RPC endpoint socket, defaults to \"127.0.0.1:3000\" for main net and \"127.0.0.1:3100\" for test net.  */
        
        
          if (update_arg( (void *)&(args_info->rpc_arg), 
               &(args_info->rpc_orig), &(args_info->rpc_given),
              &(local_args_info.rpc_given), optarg, 0, 0, ARG_STRING,
              check_ambiguity, override, 0, 0,
              "rpc", 'r',
              additional_error))
            goto failure;
        
          break;
        case 'c':	/* Configuration file, default is \"config.toml\", in testnet \"testnet3_chain.db3\".  */
        
        
          if (update_arg( (void *)&(args_info->config_arg), 
               &(args_info->config_orig), &(args_info->config_given),
              &(local_args_info.config_given), optarg, 0, 0, ARG_STRING,
              check_ambiguity, override, 0, 0,
              "config", 'c',
              additional_error))
            goto failure;
        
          break;
        case 't':	/* test the configuration file (check for correct syntax).  */
        
        
          if (update_arg( 0 , 
               0 , &(args_info->test_given),
              &(local_args_info.test_given), optarg, 0, 0, ARG_NO,
              check_ambiguity, override, 0, 0,
              "test", 't',
              additional_error))
            goto failure;
        
          break;

        case 0:	/* Long option with no short option */
          if (strcmp (long_options[option_index].name, "detailed-help") == 0) {
            cmdline_parser_print_detailed_help ();
            cmdline_parser_free (&local_args_info);
            exit (EXIT_SUCCESS);
          }

          /* Do not allow peers (for testing purposes, do not use in production).  */
          if (strcmp (long_options[option_index].name, "isolated") == 0)
          {
          
          
            if (update_arg( 0 , 
                 0 , &(args_info->isolated_given),
                &(local_args_info.isolated_given), optarg, 0, 0, ARG_NO,
                check_ambiguity, override, 0, 0,
                "isolated", '-',
                additional_error))
              goto failure;
          
          }
          /* Use temporary database (for testing purposes, do not use in production).  */
          else if (strcmp (long_options[option_index].name, "temporary") == 0)
          {
          
          
            if (update_arg( 0 , 
                 0 , &(args_info->temporary_given),
                &(local_args_info.temporary_given), optarg, 0, 0, ARG_NO,
                check_ambiguity, override, 0, 0,
                "temporary", '-',
                additional_error))
              goto failure;
          
          }
          /* Enable testnet.  */
          else if (strcmp (long_options[option_index].name, "testnet") == 0)
          {
          
          
            if (update_arg( 0 , 
                 0 , &(args_info->testnet_given),
                &(local_args_info.testnet_given), optarg, 0, 0, ARG_NO,
                check_ambiguity, override, 0, 0,
                "testnet", '-',
                additional_error))
              goto failure;
          
          }
          /* Don't mine transactions (in case of bugs).  */
          else if (strcmp (long_options[option_index].name, "disable-tx-mining") == 0)
          {
          
          
            if (update_arg( 0 , 
                 0 , &(args_info->disable_tx_mining_given),
                &(local_args_info.disable_tx_mining_given), optarg, 0, 0, ARG_NO,
                check_ambiguity, override, 0, 0,
                "disable-tx-mining", '-',
                additional_error))
              goto failure;
          
          }
          /* specify chain data file.  */
          else if (strcmp (long_options[option_index].name, "chain-db") == 0)
          {
          
          
            if (update_arg( (void *)&(args_info->chain_db_arg), 
                 &(args_info->chain_db_orig), &(args_info->chain_db_given),
                &(local_args_info.chain_db_given), optarg, 0, 0, ARG_STRING,
                check_ambiguity, override, 0, 0,
                "chain-db", '-',
                additional_error))
              goto failure;
          
          }
          /* specify data file.  */
          else if (strcmp (long_options[option_index].name, "peers-db") == 0)
          {
          
          
            if (update_arg( (void *)&(args_info->peers_db_arg), 
                 &(args_info->peers_db_orig), &(args_info->peers_db_given),
                &(local_args_info.peers_db_given), optarg, 0, 0, ARG_STRING,
                check_ambiguity, override, 0, 0,
                "peers-db", '-',
                additional_error))
              goto failure;
          
          }
          /* Websocket port.  */
          else if (strcmp (long_options[option_index].name, "ws-port") == 0)
          {
          
          
            if (update_arg( (void *)&(args_info->ws_port_arg), 
                 &(args_info->ws_port_orig), &(args_info->ws_port_given),
                &(local_args_info.ws_port_given), optarg, 0, 0, ARG_INT,
                check_ambiguity, override, 0, 0,
                "ws-port", '-',
                additional_error))
              goto failure;
          
          }
          /* TLS certificate file for public websocket endpoint.  */
          else if (strcmp (long_options[option_index].name, "ws-tls-cert") == 0)
          {
          
          
            if (update_arg( (void *)&(args_info->ws_tls_cert_arg), 
                 &(args_info->ws_tls_cert_orig), &(args_info->ws_tls_cert_given),
                &(local_args_info.ws_tls_cert_given), optarg, 0, 0, ARG_STRING,
                check_ambiguity, override, 0, 0,
                "ws-tls-cert", '-',
                additional_error))
              goto failure;
          
          }
          /* TLS private key file for public websocket endpoint.  */
          else if (strcmp (long_options[option_index].name, "ws-tls-key") == 0)
          {
          
          
            if (update_arg( (void *)&(args_info->ws_tls_key_arg), 
                 &(args_info->ws_tls_key_orig), &(args_info->ws_tls_key_given),
                &(local_args_info.ws_tls_key_given), optarg, 0, 0, ARG_STRING,
                check_ambiguity, override, 0, 0,
                "ws-tls-key", '-',
                additional_error))
              goto failure;
          
          }
          /* Honor 'X-Forwarded-For' header to determine peer IP. Intended use for reverse-proxies..  */
          else if (strcmp (long_options[option_index].name, "ws-x-forwarded-for") == 0)
          {
          
          
            if (update_arg( 0 , 
                 0 , &(args_info->ws_x_forwarded_for_given),
                &(local_args_info.ws_x_forwarded_for_given), optarg, 0, 0, ARG_NO,
                check_ambiguity, override, 0, 0,
                "ws-x-forwarded-for", '-',
                additional_error))
              goto failure;
          
          }
          /* Bind to loopback interface. Intended use for reverse-proxies..  */
          else if (strcmp (long_options[option_index].name, "ws-bind-localhost") == 0)
          {
          
          
            if (update_arg( 0 , 
                 0 , &(args_info->ws_bind_localhost_given),
                &(local_args_info.ws_bind_localhost_given), optarg, 0, 0, ARG_NO,
                check_ambiguity, override, 0, 0,
                "ws-bind-localhost", '-',
                additional_error))
              goto failure;
          
          }
          /* Public JSON RPC endpoint socket, disabled by default.  */
          else if (strcmp (long_options[option_index].name, "publicrpc") == 0)
          {
          
          
            if (update_arg( (void *)&(args_info->publicrpc_arg), 
                 &(args_info->publicrpc_orig), &(args_info->publicrpc_given),
                &(local_args_info.publicrpc_given), optarg, 0, 0, ARG_STRING,
                check_ambiguity, override, 0, 0,
                "publicrpc", '-',
                additional_error))
              goto failure;
          
          }
          /* Solo mining stratum.  */
          else if (strcmp (long_options[option_index].name, "stratum") == 0)
          {
          
          
            if (update_arg( (void *)&(args_info->stratum_arg), 
                 &(args_info->stratum_orig), &(args_info->stratum_given),
                &(local_args_info.stratum_given), optarg, 0, 0, ARG_STRING,
                check_ambiguity, override, 0, 0,
                "stratum", '-',
                additional_error))
              goto failure;
          
          }
          /* Shorthand for --publicrpc=0.0.0.0:3001.  */
          else if (strcmp (long_options[option_index].name, "enable-public") == 0)
          {
          
          
            if (update_arg( 0 , 
                 0 , &(args_info->enable_public_given),
                &(local_args_info.enable_public_given), optarg, 0, 0, ARG_NO,
                check_ambiguity, override, 0, 0,
                "enable-public", '-',
                additional_error))
              goto failure;
          
          }
          /* dump the current configuration to stdout.  */
          else if (strcmp (long_options[option_index].name, "dump-config") == 0)
          {
          
          
            if (update_arg( 0 , 
                 0 , &(args_info->dump_config_given),
                &(local_args_info.dump_config_given), optarg, 0, 0, ARG_NO,
                check_ambiguity, override, 0, 0,
                "dump-config", '-',
                additional_error))
              goto failure;
          
          }
          
          break;
        case '?':	/* Invalid option.  */
          /* `getopt_long' already printed an error message.  */
          goto failure;

        default:	/* bug: option not considered.  */
          fprintf (stderr, "%s: option unknown: %c%s\n", CMDLINE_PARSER_PACKAGE, c, (additional_error ? additional_error : ""));
          abort ();
        } /* switch */
    } /* while */



	FIX_UNUSED(check_required);

  cmdline_parser_release (&local_args_info);

  if ( error_occurred )
    return (EXIT_FAILURE);

  return 0;

failure:
  
  cmdline_parser_release (&local_args_info);
  return (EXIT_FAILURE);
}
/* vim: set ft=c noet ts=8 sts=8 sw=8 tw=80 nojs spell : */
