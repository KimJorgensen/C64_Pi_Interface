--------------------------------------------------------------------------------
--
-- Test Bench for C64 - Pi Interface
--
--------------------------------------------------------------------------------
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
 
ENTITY test_bench IS
END test_bench;
 
ARCHITECTURE behavior OF test_bench IS 
    COMPONENT cpi
    PORT(
         addr : IN  std_logic_vector(15 downto 0);
         data : INOUT  std_logic_vector(7 downto 0);
         n_dma : OUT  std_logic;
         ba : IN  std_logic;
         n_roml : IN  std_logic;
         n_romh : IN  std_logic;
         n_io1 : IN  std_logic;
         n_io2 : IN  std_logic;
         n_wr : IN  std_logic;
         n_irq : OUT  std_logic;
         n_nmi : OUT  std_logic;
         n_reset : IN  std_logic;
         d_clk : IN  std_logic;
         clk : IN  std_logic;
         phi2 : IN  std_logic;
         n_exrom : OUT  std_logic;
         n_game : OUT  std_logic;
         n_sw2 : IN  std_logic;
         n_led : OUT  std_logic;
         sa : IN std_logic_vector (5 downto 0);
         sd : OUT std_logic_vector (15 downto 0);
         sdrr : OUT std_logic;
         sdwr : OUT std_logic;
         n_soe : IN std_logic;
         n_swe : IN std_logic;
         gpio : IN std_logic_vector (27 downto 26);
         n_flash_cs : OUT  std_logic;
         n_ram_cs : OUT  std_logic;
         sck : OUT  std_logic;
         miso : OUT  std_logic;
         n_wp : OUT  std_logic;
         mosi : OUT  std_logic;
         n_hold : OUT  std_logic;
         tp1 : OUT  std_logic;
         tp2 : OUT  std_logic;
         tp3 : OUT  std_logic;
         tp4 : OUT  std_logic;
         tp5 : OUT  std_logic
        );
    END COMPONENT;

   --Inputs
   signal addr : std_logic_vector(15 downto 0) := (others => '0');
   signal ba : std_logic := '1';
   signal n_roml : std_logic := '1';
   signal n_romh : std_logic := '1';
   signal n_io1 : std_logic := '1';
   signal n_io2 : std_logic := '1';
   signal n_wr : std_logic := '1';
   signal n_reset : std_logic := '0';
   signal d_clk : std_logic := '0';
   signal clk : std_logic := '0';
   signal phi2 : std_logic := '0';
   signal n_sw2 : std_logic := '1';
   signal sa : std_logic_vector(5 downto 0) := (others => '1');
   signal n_soe : std_logic := '1';
   signal n_swe : std_logic := '1';
   signal gpio : std_logic_vector(27 downto 26) := (others => '1');

	--BiDirs
   signal data : std_logic_vector(7 downto 0);

 	--Outputs
   signal n_dma : std_logic;
   signal n_irq : std_logic;
   signal n_nmi : std_logic;
   signal n_exrom : std_logic;
   signal n_game : std_logic;
   signal n_led : std_logic;
   signal sd : std_logic_vector(15 downto 0);
   signal sdrr : std_logic;
   signal sdwr : std_logic;
   signal n_flash_cs : std_logic;
   signal n_ram_cs : std_logic;
   signal sck : std_logic;
   signal miso : std_logic;
   signal n_wp : std_logic;
   signal mosi : std_logic;
   signal n_hold : std_logic;
   signal tp1 : std_logic;
   signal tp2 : std_logic;
   signal tp3 : std_logic;
   signal tp4 : std_logic;
   signal tp5 : std_logic;

   -- Internal signals
   signal smi_clk : std_logic;
   signal last_sdrr : std_logic := '0';

   -- Clock period definitions
   constant clk_period : time := 40 ns;
   constant d_clk_period : time := 126.8716 ns;
   constant phi2_period : time := d_clk_period * 8;   
   constant smi_clk_period : time := 4 ns;

    -- MOS6510 clock timing
   constant t_aes_max : time := 75 ns;
   constant t_hrw_typ : time := 30 ns;  -- Same as t_ha
 
BEGIN
   uut: cpi PORT MAP (
          addr => addr,
          data => data,
          n_dma => n_dma,
          ba => ba,
          n_roml => n_roml,
          n_romh => n_romh,
          n_io1 => n_io1,
          n_io2 => n_io2,
          n_wr => n_wr,
          n_irq => n_irq,
          n_nmi => n_nmi,
          n_reset => n_reset,
          d_clk => d_clk,
          clk => clk,
          phi2 => phi2,
          n_exrom => n_exrom,
          n_game => n_game,
          n_sw2 => n_sw2,
          n_led => n_led,
          sa => sa,
          sd => sd,
          sdrr => sdrr,
          sdwr => sdwr,
          n_soe => n_soe,
          n_swe => n_swe,
          gpio => gpio,
          n_flash_cs => n_flash_cs,
          n_ram_cs => n_ram_cs,
          sck => sck,
          miso => miso,
          n_wp => n_wp,
          mosi => mosi,
          n_hold => n_hold,
          tp1 => tp1,
          tp2 => tp2,
          tp3 => tp3,
          tp4 => tp4,
          tp5 => tp5
        );

   -- Clock process definitions
   clk_process :process
   begin
		clk <= '0';
		wait for clk_period/2;
		clk <= '1';
		wait for clk_period/2;
   end process; 

   d_clk_process :process
   begin
		d_clk <= '1';
		wait for d_clk_period/2;
		d_clk <= '0';
		wait for d_clk_period/2;
   end process;
   
   phi2_process :process
   begin
		phi2 <= '0';
		wait for phi2_period/2;
		phi2 <= '1';
		wait for phi2_period/2;
   end process;

   smi_clk_process :process
   begin
		smi_clk <= '0';
		wait for smi_clk_period/2;
		smi_clk <= '1';
		wait for smi_clk_period/2;
   end process;

   -- SMI read
   smi_read_process :process
   begin
        n_soe <= '1';
        wait until rising_edge(smi_clk);
        if sdrr = '1' then
            if last_sdrr = '0' then
                last_sdrr <= '1';
                wait for 30 ns;
            end if;
            wait for 30 ns;
            n_soe <= '0';
            wait for 60 ns;
            n_soe <= '1';
            wait for 30 ns;
        else
            last_sdrr <= '0';
        end if;
   end process;

   -- Stimulus process
   stim_proc: process
   begin		
      -- hold reset state for 100 ns.
      wait for 100 ns;
      n_reset <= '0';
      data  <= (others => 'Z');

      wait for 20 ns;
      n_reset <= '1';

      -- write cycle
      wait until rising_edge(phi2);
      wait for t_aes_max;
      n_wr  <= '0';
      addr  <= x"de00";
      data  <= x"64";
      n_io1 <= '0'; -- TODO: add delay to n_io?
      wait until falling_edge(phi2);
      wait for t_hrw_typ;
      n_wr  <= '1';
      addr  <= x"0000";
      data  <= (others => 'Z');
      n_io1 <= '1';

      -- read cycle (n_io1)
      wait until rising_edge(phi2);
      wait for t_aes_max;
      n_wr  <= '1';
      addr  <= x"deff";
      data  <= x"64";
      n_io1 <= '0';
      -- wait for t_acc_max;
      wait until falling_edge(phi2);
      wait for t_hrw_typ;
      n_wr  <= '1';
      addr  <= x"0000";
      data  <= (others => 'Z');
      n_io1 <= '1';

      -- test switch
      wait for 50 ns;
      n_sw2 <= '0';    

      -- read cycle (n_io2)
      wait until rising_edge(phi2);
      wait for t_aes_max;
      n_wr  <= '1';
      addr  <= x"df5a";
      n_io2 <= '0';
      -- wait for t_acc_max;
      wait until falling_edge(phi2);
      wait for t_hrw_typ;
      addr  <= x"0000";
      n_io2 <= '1';

      wait;
   end process;
END;
