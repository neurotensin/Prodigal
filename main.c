/******************************************************************************
    PRODIGAL (PROkaryotic DynamIc Programming Genefinding ALgorithm)
    Copyright (C) 2007-2014 University of Tenum_nodesessee / UT-Battelle

    Code Author:  Doug Hyatt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include "anonymous.h"
#include "datatypes.h"
#include "dprog.h"
#include "gene.h"
#include "node.h"
#include "training.h"

#define VERSION "3.0.0-devel.1.0"
#define DATE "August, 2014"
#define TEXTSIZE 10000

void version();
void usage(char *);
void help();
int allocate_memory(unsigned char **, unsigned char **, unsigned char **,
                    struct _node **, struct _gene **, struct _gene_data **,
                    struct _preset_genome_bin *, struct _gene ***);
void parse_arguments(int, char **, char *, char *, char *, char *, char *,
                     char *, char *, int *, int *, int *, int *, int *,
                     int *, int *);
void header(int, int);
void log_text(int, char *);
int detect_input_and_handle_windows_stdin(int, int, char *);
int copy_standard_input_to_file(char *, int);
void open_files(char *, char *, char *, char *, char *, char *, FILE **,
                FILE **, FILE **, FILE **, FILE **, FILE **);

void free_variables(unsigned char *, unsigned char *, unsigned char *,
                    struct _node *, struct _gene *,
                    struct _preset_genome_bin *, struct _gene **);
void close_filehandles(FILE *, FILE *, FILE *, FILE *, FILE *, FILE *);

/* Main Function for Prodigal */
int main(int argc, char *argv[])
{

  /* Counting variables */
  int num_nodes = 0;         /* Number of potential starts/stops */
  int num_genes = 0;         /* Number of genes predicted */
  int initial_node = -1;     /* Pointer to first node in dynamic programming */
  int last_node = -1;        /* Pointer to last node (which has the score) */
  int i = 0;                 /* Loop variable */

  /* Sequence-related Variables */
  unsigned char *seq = NULL;        /* Bit string of forward sequence */
  unsigned char *rseq = NULL;       /* Bit string of reverse sequence */
  unsigned char *useq = NULL;       /* Bit string for ambiguous chars */
  char cur_header[TEXTSIZE] = "";   /* Header of current sequence */
  char short_header[TEXTSIZE] = ""; /* Shortened version of header */
  char new_header[TEXTSIZE] = "";   /* Header for next sequence in queue */
  int seq_length = 0;               /* Length of the sequence */
  int max_seq_length = 0;           /* Maximum observed sequence length */
  int num_seq = 0;                  /* Ordinal ID of this sequence */
  double seq_gc = 0.0;              /* GC content for the sequence */

  /* File names */
  char input_file[TEXTSIZE] = "";  /* Input file */
  char output_file[TEXTSIZE] = ""; /* Output file */
  char train_file[TEXTSIZE] = "";  /* Training file */
  char start_file[TEXTSIZE] = "";  /* Start file */
  char amino_file[TEXTSIZE] = "";  /* Protein translation file */
  char nuc_file[TEXTSIZE] = "";    /* Nucleotide gene sequence file */
  char summ_file[TEXTSIZE] = "";   /* Summary statistics file */

  /* File pointers */
  FILE *input_ptr = stdin;   /* Input file pointer */
  FILE *output_ptr = stdout; /* Output file pointer */
  FILE *amino_ptr = NULL;    /* Protein translation file pointer */
  FILE *nuc_ptr = NULL;      /* Nucleotide gene sequence file pointer */
  FILE *summ_ptr = NULL;     /* Summary statistics file pointer */
  FILE *start_ptr = NULL;    /* Complete start list file pointer */

  /* Command line arguments */
  int mode = 0;              /* Mode to run program in */
                             /* 0 = normal, 1 = training only, */
                             /* 2 = anonymous/metagenomic */
  int output_format = 0;     /* Output format flag */
                             /* 0 = Unspecified, 1 = Genbank, 2 = SCO, */
                             /* 3 = GFF, 4 = Sequin */
  int genetic_code = 0;      /* 0 = Auto, 1-23 = NCBI Genetic Code */
  int cross_gaps = 0;        /* 0 = genes cannot span runs of N's */
                             /* 1 = genes can span runs of N's */
  int no_partial_genes = 0;  /* 0 = Allow partial genes (default) */
                             /* 1 = Do not allow partial genes */
  int force_nonsd = 0;       /* If set to 1, force the non-Shine-Dalgarno */
                             /* RBS finder to be run */
  int quiet = 0;             /* If set to 1, turn off stderr logging */

  /* Structures */
  struct _node *nodes = NULL;              /* All starts or stops */
  struct _gene *genes = NULL;              /* Compact structure for genes */
  struct _gene **anon_genes = NULL;        /* Genes from anonymous runs */
  struct _gene_data *gene_data = NULL;     /* Text containing gene metadata */
  struct _training train_data = {0};       /* Training parameters for genome */
  struct _summary statistics = {0};        /* Summary statistics for genome */
  struct _preset_genome_bin                /* Preset Training Files */
         presets[NUM_PRESET_GENOME]        /* for Anonymous Runs    */
         = {{0}};

  /* Miscellaneous variables */
  int is_piped_input = 0;    /* Detects if input has been piped into program */
  char text[TEXTSIZE] = "";  /* String for printing formatted text */

  /* Anonymous/Metagenomic Run Variables */
  int max_preset = 0;        /* Index of best preset training file */
  double max_score = -100.0; /* Highest score from anonymous run */
  double low = 0.0;          /* Low-GC boundary for anonymous run */
  double high = 0.0;         /* High-GC boundary for anonymous run */

  /* Allocate memory for data structures */
  if (allocate_memory(&seq, &rseq, &useq, &nodes, &genes, &gene_data, presets,
                      &anon_genes) == -1)
  {
    perror("\nError: Dynamic memory allocation failed.");
    exit(1);
  }

  /* Parse and validate the command line arguments */
  parse_arguments(argc, argv, input_file, output_file, train_file,
                  amino_file, nuc_file, start_file, summ_file, &mode,
                  &output_format, &genetic_code, &no_partial_genes,
                  &cross_gaps, &force_nonsd, &quiet);

  /* Defaults for genetic code and start weight */
  train_data.start_weight = 4.35;
  if (genetic_code == 0)
  {
    train_data.trans_table = 11; /* 11 default genetic code */
  }
  else
  {
    train_data.trans_table = genetic_code;
  }

  /* Print header */
  header(quiet, mode);

  /* Look for input on stdin and handle Windows' inability to rewind stdin */
  if (mode == 0 && train_file[0] == '\0' && input_file[0] == '\0')
  {
    is_piped_input = detect_input_and_handle_windows_stdin(argc, quiet,
                                                           input_file);
  }

  /* Read in the training file (if specified) */
  if (train_file[0] != '\0')
  {
    sprintf(text, "Reading in training data from file %s...", train_file);
    log_text(quiet, text);
    if (read_training_file(train_file, &train_data) == -1)
    {
      perror("\n\nError: training file did not read correctly!");
      exit(6);
    }
    log_text(quiet, "done.\n-------------------------------------\n");
  }

  /* Check i/o files (if specified) and prepare them for reading/writing */
  open_files(input_file, output_file, start_file, amino_file, nuc_file,
             summ_file, &input_ptr, &output_ptr, &start_ptr, &amino_ptr,
             &nuc_ptr, &summ_ptr);

  /***************************************************************************
    Single Genome Training:  Read in the sequence(s) and perform the
    training on them.
  ***************************************************************************/
  if (mode == 1 || (mode == 0 && train_file[0] == '\0'))
  {
    log_text(quiet, "Reading in the sequence(s) to train...");
    seq_length = read_seq_training(input_ptr, seq, useq, &(train_data.gc),
                                   no_partial_genes, &num_seq);
    reverse_seq(seq, rseq, useq, seq_length);
    sprintf(text, "%d bp seq created, %.2f pct GC\n", seq_length,
            train_data.gc*100.0);
    log_text(quiet, text);
    check_node_allocation(&nodes, seq_length);

    /* Build the training set and score the coding of every start-stop pair */
    build_training_set_full(nodes, &train_data, &statistics, seq, rseq, useq,
                            seq_length, &num_nodes, no_partial_genes,
                            cross_gaps, num_seq, genetic_code, quiet);

    /***********************************************************************
      Determine if this organism uses Shine-Dalgarno or not and score the
      nodes appropriately.
    ***********************************************************************/
    log_text(quiet, "Examining upstream regions and training starts...");
    rbs_score(seq, rseq, seq_length, nodes, num_nodes, train_data.rbs_wt);
    train_starts_sd(seq, rseq, seq_length, nodes, num_nodes, &train_data);
    determine_sd_usage(&train_data);
    if (force_nonsd == 1)
    {
      train_data.uses_sd = 0;
    }
    if (train_data.uses_sd == 0)
    {
      train_starts_nonsd(seq, rseq, seq_length, nodes, num_nodes, &train_data);
    }
    log_text(quiet, "done.\n");

    /* If training specified, write the training file and exit. */
    if (mode == 1)
    {
      log_text(quiet, "Writing data to training file...");
      if (write_training_file(output_ptr, &train_data) != 0)
      {
        perror("\nError: could not write training file!");
        exit(12);
      }
      else
      {
        log_text(quiet, "done.\n");
        exit(0);
      }
    }

    /* Rewind input file */
    log_text(quiet, "-------------------------------------\n");
    if (fseek(input_ptr, 0, SEEK_SET) == -1)
    {
      perror("\nError: could not rewind input file.");
      exit(13);
    }
    /* Reset all the sequence/dynamic programming variables */
    memset(seq, 0, (seq_length /4 + 1) * sizeof(unsigned char));
    memset(rseq, 0, (seq_length / 4 + 1) * sizeof(unsigned char));
    memset(useq, 0, (seq_length / 8 + 1) * sizeof(unsigned char));
    memset(nodes, 0, num_nodes * sizeof(struct _node));
    memset(&statistics, 0, sizeof(struct _summary));
    num_nodes = 0;
    seq_length = 0;
    initial_node = -1;
    last_node = -1;
    num_seq = 0;
  }

  /* Initialize the training files for an anonymous request */
  else if (mode == 2)
  {
    log_text(quiet, "Initializing preset training files...");
    initialize_preset_genome_bins(presets);
    log_text(quiet, "done.\n-------------------------------------\n");
  }

  /************************************************************/
  /*                     Gene Prediction Phase                */
  /************************************************************/
  if (mode == 2)
  {
    log_text(quiet, "Mode: Anonymous, Phase: Gene Finding\n");
  }
  else
  {
    log_text(quiet, "Mode: Normal, Phase: Gene Finding\n");
  }

  /* Read and process each sequence in the file in succession */
  sprintf(cur_header, "Prodigal_Seq_1");
  sprintf(new_header, "Prodigal_Seq_2");
  while ((seq_length = next_seq_multi(input_ptr, seq, useq, &num_seq, &seq_gc,
         cur_header, new_header)) != -1)
  {
    reverse_seq(seq, rseq, useq, seq_length);

    sprintf(text, "Finding genes in sequence #%d (%d bp)...", num_seq,
            seq_length);
    log_text(quiet, text);

    /* Reallocate memory if this is the biggest sequence we've seen */
    if (seq_length > max_seq_length)
    {
      check_node_allocation(&nodes, seq_length);
      max_seq_length = seq_length;
    }

    /* Calculate short header for this sequence */
    calc_short_header(cur_header, short_header, num_seq);

    if (mode != 2) /* Single Genome Version */
    {

      /***********************************************************************
        Find all the potential starts and stops, sort them, and create a
        comprehensive list of nodes for dynamic programming.
      ***********************************************************************/
      num_nodes = add_nodes(seq, rseq, useq, seq_length, nodes,
                            no_partial_genes, cross_gaps,
                            train_data.trans_table);
      qsort(nodes, num_nodes, sizeof(struct _node), &compare_nodes);

      /***********************************************************************
        Second dynamic programming, using the dicodon statistics as the
        scoring function.
      ***********************************************************************/
      score_nodes(seq, rseq, seq_length, nodes, num_nodes, &train_data,
                  no_partial_genes, mode);
      record_overlapping_starts(nodes, num_nodes, train_data.start_weight, 1);
      last_node = dynamic_programming(nodes, num_nodes, train_data.bias,
                                      train_data.start_weight, 1);
      initial_node = find_first_node_from_last_node(nodes, last_node);
      num_genes = add_genes(genes, nodes, initial_node);
      adjust_starts(genes, num_genes, nodes, num_nodes,
                    train_data.start_weight);
      record_gene_data(genes, gene_data, num_genes, nodes, &train_data,
                       num_seq);
      log_text(quiet, "done.\n");

      /* Output the genes */
      print_genes(output_ptr, genes, gene_data, num_genes, nodes, seq_length,
                  output_format, num_seq, mode, NULL, &train_data, cur_header,
                  short_header, VERSION);
      fflush(output_ptr);
      write_translations(amino_ptr, genes, gene_data, num_genes, nodes, seq,
                         rseq, useq, seq_length, train_data.trans_table,
                         num_seq, short_header);
      write_nucleotide_seqs(nuc_ptr, genes, gene_data, num_genes, nodes,
                            seq, rseq, useq, seq_length, num_seq,
                            short_header);
      write_start_file(start_ptr, nodes, num_nodes, &train_data, num_seq,
                       seq_length, mode, NULL, VERSION, cur_header);
    }

    else /* Anonymous (Metagenomic) Version */
    {
      low = 0.88495 * seq_gc - 0.0102337;
      if (low > 0.65)
      {
        low = 0.65;
      }
      high = 0.86596 * seq_gc + .1131991;
      if (high < 0.35)
      {
        high = 0.35;
      }

      max_score = -100.0;
      for (i = 0; i < NUM_PRESET_GENOME; i++)
      {
        if (i == 0 || presets[i].data->trans_table !=
            presets[i-1].data->trans_table)
        {
          memset(nodes, 0, num_nodes * sizeof(struct _node));
          num_nodes = add_nodes(seq, rseq, useq, seq_length, nodes,
                                no_partial_genes, cross_gaps,
                                presets[i].data->trans_table);
          qsort(nodes, num_nodes, sizeof(struct _node), &compare_nodes);
        }
        if (presets[i].data->gc < low || presets[i].data->gc > high)
        {
          continue;
        }
        reset_node_scores(nodes, num_nodes);
        score_nodes(seq, rseq, seq_length, nodes, num_nodes, presets[i].data,
                    no_partial_genes, mode);
        record_overlapping_starts(nodes, num_nodes,
                                  presets[i].data->start_weight, 1);
        last_node = dynamic_programming(nodes, num_nodes,
                                        presets[i].data->bias,
                                        presets[i].data->start_weight, 1);
        initial_node = find_first_node_from_last_node(nodes, last_node);
        if (nodes[last_node].score > max_score)
        {
          max_preset = i;
          max_score = nodes[last_node].score;
          num_genes = add_genes(anon_genes[i], nodes, initial_node);
          adjust_starts(anon_genes[i], num_genes, nodes, num_nodes,
                        presets[i].data->start_weight);
          record_gene_data(anon_genes[i], gene_data, num_genes, nodes,
                           presets[i].data, num_seq);
        }
      }

      /* Recover the nodes for the best of the runs */
      memset(nodes, 0, num_nodes * sizeof(struct _node));
      num_nodes = add_nodes(seq, rseq, useq, seq_length, nodes,
                            no_partial_genes, cross_gaps,
                            presets[max_preset].data->trans_table);
      qsort(nodes, num_nodes, sizeof(struct _node), &compare_nodes);
      score_nodes(seq, rseq, seq_length, nodes, num_nodes,
                  presets[max_preset].data, no_partial_genes, mode);
      write_start_file(start_ptr, nodes, num_nodes, presets[max_preset].data,
                       num_seq, seq_length, mode, presets[max_preset].desc,
                       VERSION, cur_header);
      log_text(quiet, "done.\n");

      /* Output the genes */
      print_genes(output_ptr, anon_genes[max_preset], gene_data, num_genes,
                  nodes, seq_length, output_format, num_seq, mode,
                  presets[max_preset].desc, presets[max_preset].data,
                  cur_header, short_header, VERSION);
      fflush(output_ptr);
      write_translations(amino_ptr, anon_genes[max_preset], gene_data,
                         num_genes, nodes, seq, rseq, useq, seq_length,
                         presets[max_preset].data->trans_table, num_seq,
                         short_header);
      write_nucleotide_seqs(nuc_ptr, anon_genes[max_preset], gene_data,
                            num_genes, nodes, seq, rseq, useq, seq_length,
                            num_seq, short_header);
    }

    /* Reset all the sequence/dynamic programming variables */
    zero_sequence(seq, rseq, useq, seq_length);
    zero_nodes(nodes, num_nodes);
    num_nodes = 0;
    seq_length = 0;
    initial_node = 0;
    strcpy(cur_header, new_header);
    sprintf(new_header, "Prodigal_Seq_%d\n", num_seq + 1);
  }

  /* Flag an error if we saw no sequences */
  if (num_seq == 0)
  {
    fprintf(stderr, "\nError:  no input sequences to analyze.\n\n");
    exit(17);
  }

  /* Cleanup: free variables, close filehandles, remove tmp file */
  free_variables(seq, rseq, useq, nodes, genes, presets, anon_genes);
  close_filehandles(input_ptr, output_ptr, start_ptr, amino_ptr, nuc_ptr,
                    summ_ptr);
  if (is_piped_input == 1 && remove(input_file) != 0)
  {
    fprintf(stderr, "Could not delete tmp file %s.\n", input_file);
    exit(18);
  }

  /* Exit successfully */
  exit(0);
}

/* Print version number and exit */
void version()
{
  printf("\nProdigal v%s: %s\n\n", VERSION, DATE);
  exit(0);
}

/* Print usage information and exit */
void usage(char *msg)
{
  fprintf(stderr, "\nError: %s\n", msg);
  fprintf(stderr, "\nUsage:  prodigal [-a protein_file] [-c] [-d mrna_file]");
  fprintf(stderr, " [-f out_format]\n");
  fprintf(stderr, "                 [-g trans_table] [-h] [-i input_file]");
  fprintf(stderr, " [-m mode] [-n]\n");
  fprintf(stderr, "                 [-o output_file] [-q] [-s start_file]");
  fprintf(stderr, " [-t train_file]\n");
  fprintf(stderr, "                 [-v] [-w summ_file] [-z]\n");
  fprintf(stderr, "\nDo 'prodigal -h' for more information.\n\n");
  exit(2);
}

/* Print help message and exit */
void help()
{
  char spaces[50] = "                          ";
  printf("\nUsage:  prodigal [-a protein_file] [-c] [-d mrna_file]");
  printf(" [-f out_format]\n");
  printf("                 [-g trans_table] [-h] [-i input_file]");
  printf(" [-m mode] [-n]\n");
  printf("                 [-o output_file] [-q] [-s start_file]");
  printf(" [-t train_file]\n");
  printf("                 [-v] [-w summ_file] [-z]\n");
  printf("\nGene Modeling Parameters\n\n");
  printf("  -m, --mode:           Specify mode (normal, train, or anon).\n");
  printf("%snormal:   Single genome, any number of\n", spaces);
  printf("%s          sequences. (Default)\n", spaces);
  printf("%strain:    Do only training.  Input should\n", spaces);
  printf("%s          be multiple FASTA of one or more\n", spaces);
  printf("%s          closely related genomes.  Output\n", spaces);
  printf("%s          is a training file.\n", spaces);
  printf("%sanon:     Anonymous sequences, analyze using\n", spaces);
  printf("%s          preset training files, ideal for\n", spaces);
  printf("%s          metagenomic data or single short\n", spaces);
  printf("%s          sequences.\n", spaces);
  printf("  -g, --trans_table:    Specify a translation table to use.\n");
  printf("%sauto: Tries 11 then 4 (Default)\n", spaces);
  printf("%s11:   Standard Bacteria/Archaea\n", spaces);
  printf("%s4:    Mycoplasma/Spiroplasma\n", spaces);
  printf("%s#:    Other genetic codes 1-25\n", spaces);
  printf("  -c, --nopartial:      Closed ends.  Do not allow partial genes\n");
  printf("                        (genes that run off edges or into gaps.)\n");
  printf("  -z, --nogaps:         Do not treat runs of N's as gaps.  This ");
  printf("option\n");
  printf("                        will build gene models that span ");
  printf("runs of N's.\n");
  printf("  -n, --force_nonsd:    Do not use the Shine-Dalgarno RBS finder\n");
  printf("                        and force Prodigal to scan for motifs.\n");
  printf("  -t, --training_file:  Read and use the specified training file\n");
  printf("                        instead of training on the input ");
  printf("sequence(s).\n");
  printf("                        (Only usable in normal mode.)\n");
  printf("\nInput/Output Parameters\n\n");
  printf("  -i, --input_file:     Specify input file (default stdin).\n");
  printf("  -o, --output_file:    Specify output file (default stdout).\n");
  printf("  -a, --protein_file:   Write protein translations to the named");
  printf(" file.\n");
  printf("  -d, --mrna_file:      Write nucleotide sequences of genes to the");
  printf("\n");
  printf("                        named file.\n");
  printf("  -w, --summ_file:      Write summary statistics to the named ");
  printf("file.\n");
  printf("  -s, --start_file:     Write all potential genes (with scores) to");
  printf(" the\n                        named file.\n");
  printf("  -f, --output_format:  Specify output format (gbk, gff, sqn, or ");
  printf("sco).\n");
  printf("                          gff:  GFF format (Default)\n");
  printf("                          gbk:  Genbank-like format\n");
  printf("                          sqn:  Sequin feature table format\n");
  printf("                          sco:  Simple coordinate output\n");
  printf("  -q, --quiet:          Run quietly (suppress normal stderr ");
  printf("output).\n");
  printf("\nOther Parameters\n\n");
  printf("  -h, --help:     Print help menu and exit.\n");
  printf("  -v, --version:  Print version number and exit.\n\n");
  exit(0);
}

/* Allocates memory for data structures and memsets them all to 0 */
int allocate_memory(unsigned char **seq, unsigned char **rseq,
                    unsigned char **useq, struct _node **nodes,
                    struct _gene **genes, struct _gene_data **gene_data,
                    struct _preset_genome_bin *presets,
                    struct _gene ***anon_genes)
{
  int i = 0;

  *seq = (unsigned char *)calloc(MAX_SEQ/4, sizeof(unsigned char));
  *rseq = (unsigned char *)calloc(MAX_SEQ/4, sizeof(unsigned char));
  *useq = (unsigned char *)calloc(MAX_SEQ/8, sizeof(unsigned char));
  *nodes = (struct _node *)calloc(STT_NOD, sizeof(struct _node));
  *genes = (struct _gene *)calloc(MAX_GENES, sizeof(struct _gene));
  *gene_data = (struct _gene_data*)calloc(MAX_GENES,
                                          sizeof(struct _gene_data));
  *anon_genes = (struct _gene **)calloc(NUM_PRESET_GENOME,
                                        sizeof(struct _gene *));
  if (*seq == NULL || *rseq == NULL || *nodes == NULL || *genes == NULL)
  {
    return -1;
  }

  for (i = 0; i < NUM_PRESET_GENOME; i++)
  {
    (*anon_genes)[i] = (struct _gene *)calloc(MAX_GENES, sizeof(struct _gene));
    strcpy(presets[i].desc, "None");
    presets[i].data = (struct _training *)calloc(1, sizeof(struct _training));
    if (presets[i].data == NULL)
    {
      return -1;
    }
  }
  return 0;
}

/* Initialize argument variables, parse command line arguments, */
/* and validate the arguments for consistency. */
void parse_arguments(int argc, char **argv, char *input_file, char
                     *output_file, char *train_file, char *amino_file, char
                     *nuc_file, char *start_file, char *summ_file, int *mode,
                     int *output_format, int *genetic_code, int *closed, int
                     *cross_gaps, int *force_nonsd, int *quiet)
{
  int i = 0;
  int j = 0;
  char digits[10] = "0123456789";
  char err_string[TEXTSIZE] = "";

  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    {
      for (j = 0; j < strlen(argv[i]); j++)
      {
        argv[i][j] = tolower(argv[i][j]);
      }
    }
  }
  for (i = 1; i < argc; i++)
  {
    if ((i == argc-1 || argv[i+1][0] == '-') &&
       (strcmp(argv[i], "-t") == 0 ||
       strcmp(argv[i], "--training_file") == 0 ||
       strcmp(argv[i], "-a") == 0 ||
       strcmp(argv[i], "--protein_file") == 0 ||
       strcmp(argv[i], "-d") == 0 ||
       strcmp(argv[i], "--mrna_file") == 0 ||
       strcmp(argv[i], "-g") == 0 ||
       strcmp(argv[i], "--trans_table") == 0 ||
       strcmp(argv[i], "-f") == 0 ||
       strcmp(argv[i], "--output_format") == 0 ||
       strcmp(argv[i], "-s") == 0 ||
       strcmp(argv[i], "--start_file") == 0 ||
       strcmp(argv[i], "-w") == 0 ||
       strcmp(argv[i], "--summ_file") == 0 ||
       strcmp(argv[i], "-i") == 0 ||
       strcmp(argv[i], "--input_file") == 0 ||
       strcmp(argv[i], "-o") == 0 ||
       strcmp(argv[i], "--output_file") == 0 ||
       strcmp(argv[i], "-m") == 0 ||
       strcmp(argv[i], "-p") == 0 ||
       strcmp(argv[i], "--mode") == 0))
    {
      usage("-a/-d/-f/-g/-i/-m/-o/-s/-t/-w options require valid parameters.");
    }
    else if (strcmp(argv[i], "-a") == 0 ||
            strcmp(argv[i], "--protein_file") == 0)
    {
      strcpy(amino_file, argv[i+1]);
      i++;
    }
    else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--nopartial") == 0)
    {
      *closed = 1;
    }
    else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--mrna_file") == 0)
    {
      strcpy(nuc_file, argv[i+1]);
      i++;
    }
    else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--output_format")
            == 0)
    {
      if (strncmp(argv[i+1], "0", 1) == 0 || strcmp(argv[i+1], "gbk") == 0)
      {
        *output_format = 1;
      }
      else if (strncmp(argv[i+1], "2", 1) == 0 ||
               strcmp(argv[i+1], "sco") == 0)
      {
        *output_format = 2;
      }
      else if (strncmp(argv[i+1], "3", 1) == 0 ||
               strcmp(argv[i+1], "gff") == 0)
      {
        *output_format = 3;
      }
      else if (strncmp(argv[i+1], "4", 1) == 0 ||
               strcmp(argv[i+1], "sqn") == 0)
      {
        *output_format = 4;
      }
      else
      {
        usage("Invalid output format specified.");
      }
      i++;
    }
    else if (strcmp(argv[i], "-g") == 0 ||
            strcmp(argv[i], "--trans_table") == 0)
    {
      if (strcmp(argv[i+1], "auto") != 0)
      {
        *genetic_code = atoi(argv[i+1]);
        if (strspn(argv[i+1], digits) != strlen(argv[i+1]) ||
           *genetic_code < 1 || *genetic_code > 25 ||
           *genetic_code == 7 || *genetic_code == 8 ||
           (*genetic_code >= 17 && *genetic_code <= 20))
        {
          usage("Invalid or unsupported genetic code specified.");
        }
      }
      i++;
    }
    else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
    {
      help();
    }
    else if (strcmp(argv[i], "-i") == 0 ||
            strcmp(argv[i], "--input_file") == 0)
    {
      strcpy(input_file, argv[i+1]);
      i++;
    }
    else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0)
    {
      if (argv[i+1][0] == 'n')
      {
        *mode = 0;
      }
      else if (argv[i+1][0] == 't')
      {
        *mode = 1;
      }
      else if (argv[i+1][0] == 'a')
      {
        *mode = 2;
      }
      else
      {
        usage("Invalid mode specified (should be normal, train, or anon).");
      }
      i++;
    }
    else if (strcmp(argv[i], "-n") == 0 ||
             strcmp(argv[i], "--force_nonsd") == 0)
    {
      *force_nonsd = 1;
    }
    else if (strcmp(argv[i], "-o") == 0 ||
            strcmp(argv[i], "--output_file") == 0)
    {
      strcpy(output_file, argv[i+1]);
      i++;
    }
    else if (strcmp(argv[i], "-p") == 0) /* deprecated but preserved atm */
    {
      if (argv[i+1][0] == 's')
      {
        *mode = 0;
      }
      else if (argv[i+1][0] == 'm')
      {
        *mode = 2;
      }
      else
      {
        usage("Invalid procedure specified (should be single or meta).");
      }
      fprintf(stderr, "Warning: '-p meta' is deprecated.  Should use ");
      fprintf(stderr, "'-m anon' instead for metagenomic sequences.\n");
      i++;
    }
    else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
    {
      *quiet = 1;
    }
    else if (strcmp(argv[i], "-s") == 0 ||
            strcmp(argv[i], "--start_file") == 0)
    {
      strcpy(start_file, argv[i+1]);
      i++;
    }
    else if (strcmp(argv[i], "-t") == 0 ||
            strcmp(argv[i], "--training_file") == 0)
    {
      strcpy(train_file, argv[i+1]);
      i++;
    }
    else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
    {
      version();
    }
    else if (strcmp(argv[i], "-w") == 0 ||
            strcmp(argv[i], "--summ_file") == 0)
    {
      strcpy(summ_file, argv[i+1]);
      i++;
    }
    else if (strcmp(argv[i], "-z") == 0 || strcmp(argv[i], "--nogaps") == 0)
    {
      *cross_gaps = 1;
    }
    else
    {
      sprintf(err_string, "Unknown option '%s'.", argv[i]);
      usage(err_string);
    }
  }

  /* Validation of arguments checking for conflicting options */

  /* Training mode can't have output format or extra files specified. */
  if (*mode == 1 && (strlen(start_file) > 0 || strlen(nuc_file) > 0 ||
     strlen(amino_file) > 0 || strlen(summ_file) > 0 || *output_format != 0))
  {
    usage("-a/-d/-f/-s/-w options cannot be used in training mode.");
  }

  /* Normal/anonymous can't have training files specified. */
  if (*mode != 0 && strlen(train_file) > 0)
  {
    usage("Can only specify training file in normal mode.");
  }

  /* Anonymous mode can't have a specified value for genetic code. */
  /* Nor can normal mode if using a training file. */
  if ((*mode == 2 || (*mode == 0 && strlen(train_file) > 0)) &&
     *genetic_code != 0)
  {
    usage("Can't specify translation table with anon mode or training file.");
  }

  /* Once arguments have been validated, set some defaults */
  if (*output_format == 0)
  {
    *output_format = 3; /* GFF default output format */
  }
}

/* Print the header */
void header(int quiet, int mode)
{
  if (quiet == 0)
  {
    fprintf(stderr, "-------------------------------------\n");
    fprintf(stderr, "PRODIGAL v%s [%s]         \n", VERSION, DATE);
    fprintf(stderr, "Univ of Tenn / Oak Ridge National Lab\n");
    fprintf(stderr, "Doug Hyatt, Loren Hauser, et al.     \n");
    fprintf(stderr, "-------------------------------------\n");
    if (mode == 0)
    {
      fprintf(stderr, "Mode: Normal, Phase: Training\n");
    }
    else if (mode == 1)
    {
      fprintf(stderr, "Mode: Training, Phase: Training\n");
    }
    else if (mode == 2)
    {
      fprintf(stderr, "Mode: Anonymous, Phase: Training\n");
    }
  }
}

/* Print something to stderr if quiet is set to 1 */
void log_text(int quiet, char *text)
{
  if (quiet == 0)
  {
    fprintf(stderr, "%s", text);
  }
}

/* If we're in normal mode and not reading from a training file, */
/* then we have to make two passes over the sequence.  Since we  */
/* rewind after the first pass (something Windows can't do to    */
/* stdin), we copy stdin to a temp file so that we can rewind it */
/* in Windows. If there's nothing present on stdin, we print     */
/* the help message and exit. Returns a 1 if piped input is      */
/* detected, 0 otherwise.                                        */
int detect_input_and_handle_windows_stdin(int argc, int quiet,
                                           char *input_file)
{
  int file_num = 0;
  int is_piped_input = 0;
  char input_copy[TEXTSIZE] = "";
  struct stat fbuf = {0};
  pid_t pid = getpid();

  sprintf(input_copy, "tmp.prodigal.stdin.%d", pid);

  file_num = fileno(stdin);
  if (fstat(file_num, &fbuf) == -1)
  {
    fprintf(stderr, "\nError: can't fstat standard input.\n\n");
    exit(3);
  }
  if (S_ISCHR(fbuf.st_mode))
  {
    if (argc == 1)
    {
      help();
    }
    else
    {
      fprintf(stderr, "\nError: options specified but no input detected.\n\n");
      exit(4);
    }
  }
  else if (S_ISREG(fbuf.st_mode))
  {
    /* do nothing */
  }
  else if (S_ISFIFO(fbuf.st_mode))
  {
    is_piped_input = 1;
    if (copy_standard_input_to_file(input_copy, quiet) == -1)
    {
      fprintf(stderr, "\nError: can't copy stdin to file.\n\n");
      exit(5);
    }
    strcpy(input_file, input_copy);
  }
  return is_piped_input;
}

/* For piped input, we make a copy of stdin so we can rewind the file. */
int copy_standard_input_to_file(char *path, int quiet)
{
  char line[TEXTSIZE+1] = "";
  FILE *wp;

  if (quiet == 0)
  {
    fprintf(stderr, "Piped input detected, copying stdin to a tmp file...");
  }

  wp = fopen(path, "w");
  if (wp == NULL)
  {
    return -1;
  }
  while (fgets(line, TEXTSIZE, stdin) != NULL)
  {
    fprintf(wp, "%s", line);
  }
  fclose(wp);

  if (quiet == 0)
  {
    fprintf(stderr, "done!\n");
    fprintf(stderr, "-------------------------------------\n");
  }
  return 0;
}

/* Open files and set file pointers.  Exit if any files throw an error. */
void open_files(char *input_file, char *output_file, char *start_file,
                char *amino_file, char *nuc_file, char *summ_file,
                FILE **input_ptr, FILE **output_ptr, FILE **start_ptr,
                FILE **amino_ptr, FILE **nuc_ptr, FILE **summ_ptr)
{
  if (input_file[0] != '\0')
  {
    *input_ptr = fopen(input_file, "r");
    if (*input_ptr == NULL)
    {
      perror("\nError: can't open input file.");
      exit(7);
    }
  }
  if (output_file[0] != '\0')
  {
    *output_ptr = fopen(output_file, "w");
    if (*output_ptr == NULL)
    {
      perror("\nError: can't open output file.");
      exit(8);
    }
  }
  if (start_file[0] != '\0')
  {
    *start_ptr = fopen(start_file, "w");
    if (*start_ptr == NULL)
    {
      perror("\nError: can't open start file.");
      exit(8);
    }
  }
  if (amino_file[0] != '\0')
  {
    *amino_ptr = fopen(amino_file, "w");
    if (*amino_ptr == NULL)
    {
      perror("\nError: can't open translation file.");
      exit(8);
    }
  }
  if (nuc_file[0] != '\0')
  {
    *nuc_ptr = fopen(nuc_file, "w");
    if (*nuc_ptr == NULL)
    {
      perror("\nError: can't open gene nucleotide file.");
      exit(8);
    }
  }
  if (summ_file[0] != '\0')
  {
    *summ_ptr = fopen(summ_file, "w");
    if (*summ_ptr == NULL)
    {
      perror("\nError: can't open summary file.");
      exit(8);
    }
  }
}

/* Free all variables */
void free_variables(unsigned char *seq, unsigned char *rseq,
                    unsigned char *useq, struct _node *nodes,
                    struct _gene *genes, struct _preset_genome_bin *presets,
                    struct _gene **anon_genes)
{
  int i = 0;

  if (seq != NULL)
  {
    free(seq);
  }
  if (rseq != NULL)
  {
    free(rseq);
  }
  if (useq != NULL)
  {
    free(useq);
  }
  if (nodes != NULL)
  {
    free(nodes);
  }
  if (genes != NULL)
  {
    free(genes);
  }
  for (i = 0; i < NUM_PRESET_GENOME; i++)
  {
    if (presets[i].data != NULL)
    {
      free(presets[i].data);
    }
    if (anon_genes[i] != NULL)
    {
      free(anon_genes[i]);
    }
  }
  if (anon_genes != NULL)
  {
    free(anon_genes);
  }
}

/* Close all the filehandles */
void close_filehandles(FILE *input_ptr, FILE *output_ptr, FILE *start_ptr,
                       FILE *amino_ptr, FILE *nuc_ptr, FILE *summ_ptr)
{
  if (input_ptr != stdin)
  {
    fclose(input_ptr);
  }
  if (output_ptr != stdout)
  {
    fclose(output_ptr);
  }
  if (start_ptr != NULL)
  {
    fclose(start_ptr);
  }
  if (amino_ptr != NULL)
  {
    fclose(amino_ptr);
  }
  if (nuc_ptr != NULL)
  {
    fclose(nuc_ptr);
  }
  if (summ_ptr != NULL)
  {
    fclose(summ_ptr);
  }
}
