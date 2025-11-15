-- Basic settings
vim.opt.number = true
vim.opt.relativenumber = true
vim.opt.mouse = "a"
vim.opt.ignorecase = true
vim.opt.smartcase = true
vim.opt.splitright = true
vim.opt.splitbelow = true

-- Tab and indentation
vim.opt.tabstop = 2
vim.opt.shiftwidth = 2
vim.opt.expandtab = true
vim.wo.foldmethod = "expr"
vim.wo.foldexpr = "v:lua.vim.treesitter.foldexpr()"
vim.opt.foldtext = ""
vim.opt.foldlevel = 99
vim.opt.foldlevelstart = 5

-- Visual settings
vim.opt.termguicolors = true
vim.opt.cursorline = true
vim.opt.signcolumn = "yes"

-- Set leader key
vim.g.mapleader = " "
vim.opt.clipboard = "unnamedplus"
vim.keymap.set({ "n", "v" }, "d", '"_d', { noremap = true })

local lazypath = vim.fn.stdpath("data") .. "/lazy/lazy.nvim"
if not vim.loop.fs_stat(lazypath) then
	vim.fn.system({
		"git",
		"clone",
		"--filter=blob:none",
		"https://github.com/folke/lazy.nvim.git",
		"--branch=stable",
		lazypath,
	})
end

vim.opt.rtp:prepend(lazypath)

local plugins = {
	{ "nvim-lua/plenary.nvim" },
	{ "nvim-tree/nvim-web-devicons" },

	-- Color scheme
	{ "tiagovla/tokyodark.nvim" },

	-- UI
	{ "nvim-lualine/lualine.nvim" },
	{ "nvim-tree/nvim-tree.lua" },
	{ "altermo/nwm", branch = "x11" },
	-- {
	-- 	"lukas-reineke/indent-blankline.nvim",
	-- 	main = "ibl",
	-- 	opts = {},
	-- },
	--
	{
		"mg979/vim-visual-multi",
		event = "VeryLazy",
		keys = {
			{ "<C-n>", "<Plug>(VM-Find-Under)", mode = "n" },
			{ "<C-n>", "<Plug>(VM-Find-Subword-Under)", mode = "v" },
		},
	},

	-- Fuzzy finder
	{ "nvim-telescope/telescope.nvim" },
	{ "nvim-telescope/telescope-fzf-native.nvim", build = "make" },

	-- Treesitter for better syntax highlighting
	{ "nvim-treesitter/nvim-treesitter", build = ":TSUpdate" },

	-- LSP (Language Server Protocol)
	{ "neovim/nvim-lspconfig" },
	{ "mason-org/mason.nvim" },
	{ "mason-org/mason-lspconfig.nvim" },
	{ "WhoIsSethDaniel/mason-tool-installer.nvim" },

	{
		"mfussenegger/nvim-dap",
		event = "VeryLazy",
		dependencies = {
			"rcarriga/nvim-dap-ui",
			"nvim-neotest/nvim-nio",
			"jay-babu/mason-nvim-dap.nvim",
		},
		config = function()
			require("mason-nvim-dap").setup({
				ensure_installed = { "cppdbg", "codelldb" },
				automatic_installation = true,
			})

			local dap = require("dap")
			require("dapui").setup()

			vim.keymap.set("n", "<leader>db", dap.toggle_breakpoint, { desc = "Debug: Breakpoint" })
			vim.keymap.set("n", "<leader>dc", dap.continue, { desc = "Debug: Continue" })
			vim.keymap.set("n", "<leader>dC", dap.run_to_cursor, { desc = "Debug: Run to cursor" })
			vim.keymap.set("n", "<leader>dT", dap.terminate, { desc = "Debug: Terminate" })
		end,
	},

	-- Formatting
	{ "stevearc/conform.nvim" },

	{
		"akinsho/toggleterm.nvim",
		version = "*",
		config = function()
			require("toggleterm").setup({
				size = 20,
				open_mapping = [[<A-3>]], -- Ctrl+\ to toggle
				hide_numbers = true,
				start_in_insert = true,
				persist_size = true,
				direction = "float", -- or "vertical", "float", "window"
				close_on_exit = true,
				shell = vim.o.shell,
			})
		end,
	},

	-- Autocompletion
	{
		"saghen/blink.cmp",
		version = "1.*",
		event = "InsertEnter",
		dependencies = {
			"rafamadriz/friendly-snippets",
			"giuxtaposition/blink-cmp-copilot",
		},
		opts = {
			enabled = function()
				return vim.bo.buftype ~= "prompt"
			end,
			keymap = { preset = "super-tab" },
			appearance = {
				use_nvim_cmp_as_default = true,
				nerd_font_variant = "mono",
			},
			sources = {
				default = { "lsp", "path", "snippets", "buffer", "copilot" },
				providers = {
					copilot = {
						name = "copilot",
						module = "blink-cmp-copilot",
						score_offset = 100,
						async = true,
					},
				},
			},
			trigger = {
				prefetch_on_insert = true,
			},
			completion = {
				accept = { auto_brackets = { enabled = true } },
				documentation = {
					auto_show = true,
					auto_show_delay_ms = 50,
					window = { border = "rounded" },
				},
				ghost_text = { enabled = true },
				menu = { border = "rounded" },
			},
			signature = { enabled = true },
		},
	},

	{ "rafamadriz/friendly-snippets" },
	{
		"L3MON4D3/LuaSnip",
	},

	{
		"zbirenbaum/copilot.lua",
		cmd = "Copilot",
		event = "InsertEnter",
		config = function()
			require("copilot").setup({
				suggestion = { enabled = true },
				panel = { enabled = true },
			})
		end,
	},

	{
		"giuxtaposition/blink-cmp-copilot",
	},

	{ "stevearc/dressing.nvim" },

	{
		"MeanderingProgrammer/render-markdown.nvim",
		opts = { file_types = { "markdown" } },
	},

	{
		"MunifTanjim/nui.nvim",
	},

	{
		"numToStr/Comment.nvim",
		config = function()
			require("Comment").setup()
		end,
	},

	{
		"lewis6991/gitsigns.nvim",
		config = function()
			require("gitsigns").setup()
		end,
	},

	{
		"folke/flash.nvim",
		event = "VeryLazy",
		opts = {
			search = {
				max_length = false, -- Allow unlimited characters
				mode = "exact", -- Exact match (not fuzzy)
			},
		},
		keys = {
			{
				"s",
				mode = { "n", "x", "o" },
				function()
					require("flash").jump()
				end,
				desc = "Flash",
			},
			{
				"S",
				mode = { "n", "x", "o" },
				function()
					require("flash").treesitter()
				end,
				desc = "Flash Treesitter",
			},
		},
	},
	{
		"folke/which-key.nvim",
		event = "VeryLazy",
		config = function()
			require("which-key").setup()
		end,
	},
	{
		"folke/trouble.nvim",
		keys = {
			{ "<leader>xx", "<cmd>Trouble diagnostics toggle<cr>", desc = "Diagnostics (Trouble)" },
		},
		config = function()
			require("trouble").setup()
		end,
	},
	{
		"kylechui/nvim-surround",
		version = "*",
		event = "VeryLazy",
		config = function()
			require("nvim-surround").setup({})
		end,
	},

	{
		"vyfor/cord.nvim",
		build = ":Cord update",
		-- opts = {}
	},
}

require("lazy").setup(plugins)

-- Color scheme
vim.cmd.colorscheme("tokyodark")

-- Status line
require("lualine").setup()

-- File tree
require("nvim-tree").setup()

-- Telescope
require("telescope").setup()

vim.api.nvim_create_autocmd("FocusLost", {
	command = "silent! wa",
	desc = "Auto-save on focus lost",
})

local Terminal = require("toggleterm.terminal").Terminal

local build_term = Terminal:new({ cmd = "./build_nvim.sh", display_name = "Build" })
local test_term = Terminal:new({ cmd = "./test_nvim.sh", display_name = "Test" })

vim.keymap.set("n", "<leader>b", function()
	build_term:toggle()
end, { desc = "Build" })
vim.keymap.set("n", "<leader>t", function()
	test_term:toggle()
end, { desc = "Test" })

require("mason").setup()
require("mason-lspconfig").setup({
	ensure_installed = {
		"rust_analyzer", -- Rust
		"ts_ls", -- TypeScript/JavaScript
		"eslint", -- JS/TS linting
		"tailwindcss", -- Tailwind (if needed)
		"clangd", -- C++
		"lua_ls", -- Lua
		"pyright", -- Python (bonus)
	},
	auto_install = true,
})

-- LSP server configuration
local lsp_capabilities = require("blink.cmp").get_lsp_capabilities()

-- Configure individual servers (optional - for custom settings)
vim.lsp.config("clangd", {
	cmd = { "clangd", "--background-index", "-j=12", "--clang-tidy" },
	capabilities = lsp_capabilities,
	init_options = {
		clangdOnConfigChanged = "restart",
	},
})

vim.lsp.config("lua_ls", {
	settings = {
		Lua = {
			diagnostics = {
				globals = { "vim" },
			},
			workspace = {
				library = vim.api.nvim_get_runtime_file("", true),
			},
			runtime = {
				version = "LuaJIT",
			},
			telemetry = {
				enable = false,
			},
		},
	},
})

-- Enable all servers at once
vim.lsp.enable({
	"rust_analyzer",
	"ts_ls",
	"clangd",
	"lua_ls",
	"pyright",
	"eslint",
})

-- LSP keybindings (setup after enable)
local function setup_lsp_keybinds(event)
	local bufnr = event.buf
	vim.keymap.set("n", "K", vim.lsp.buf.hover, { buffer = bufnr, desc = "Hover" })
	vim.keymap.set("n", "gd", vim.lsp.buf.definition, { buffer = bufnr, desc = "Go to Definition" })
	vim.keymap.set("n", "gr", vim.lsp.buf.references, { buffer = bufnr, desc = "Go to References" })
	vim.keymap.set("n", "<leader>rn", vim.lsp.buf.rename, { buffer = bufnr, desc = "Rename" })
	vim.keymap.set("n", "<leader>ca", vim.lsp.buf.code_action, { buffer = bufnr, desc = "Code Action" })
end

vim.api.nvim_create_autocmd("LspAttach", {
	callback = setup_lsp_keybinds,
})

-- Formatting
require("conform").setup({
	formatters_by_ft = {
		javascript = { "prettier" },
		typescript = { "prettier" },
		typescriptreact = { "prettier" },
		javascriptreact = { "prettier" },
		tsx = { "prettier" },
		rust = { "rustfmt" },
		cpp = { "clang-format" },
		c = { "clang-format" },
		lua = { "stylua" },
		python = { "black" },
	},
	format_on_save = {
		timeout_ms = 500,
		lsp_fallback = true,
	},
})

require("nvim-treesitter.configs").setup({
	ensure_installed = {
		"rust",
		"typescript",
		"tsx",
		"javascript",
		"cpp",
		"c",
		"cmake",
		"lua",
		"python",
		"json",
		"css",
		"html",
		"svelte",
	},
	auto_install = true,
	highlight = { enable = true },
	indent = { enable = true },
})

vim.filetype.add({
	extension = {
		svx = "markdown",
	},
})

-- keybinds

local builtin = require("telescope.builtin")
vim.keymap.set("n", "<leader>ff", builtin.find_files)
vim.keymap.set("n", "<leader>fg", builtin.live_grep)
vim.keymap.set("n", "<leader>fb", builtin.buffers)
vim.keymap.set("n", "<leader>fh", builtin.help_tags)

-- File tree
vim.keymap.set("n", "<leader>e", ":NvimTreeToggle<CR>", { desc = "File explorer" })

-- Formatting
vim.keymap.set("n", "<leader>f ", function()
	require("conform").format()
end)
vim.keymap.set("v", "<leader>fo", function()
	require("conform").format()
end)

-- LSP keybindings
vim.keymap.set("n", "K", vim.lsp.buf.hover)
vim.keymap.set("n", "gd", vim.lsp.buf.definition)
vim.keymap.set("n", "gr", vim.lsp.buf.references)
vim.keymap.set("n", "<leader>rn", vim.lsp.buf.rename)
vim.keymap.set("n", "<leader>ca", vim.lsp.buf.code_action)

-- Diagnostics
vim.keymap.set("n", "[d", vim.diagnostic.goto_prev)
vim.keymap.set("n", "]d", vim.diagnostic.goto_next)

vim.keymap.set("n", "<leader>/", function()
	require("Comment.api").toggle.linewise.current()
end)
vim.keymap.set("v", "<leader>/", "<ESC><cmd>lua require('Comment.api').toggle.linewise(vim.fn.visualmode())<CR>")

vim.keymap.set("n", "<leader>'", ":nohlsearch<CR>", { desc = "Clear search highlight" })
vim.keymap.set("i", "<C-space>", function()
	require("blink.cmp").show()
end)
