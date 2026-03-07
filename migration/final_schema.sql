-- =============================================================================
-- UNIFIED DATABASE SCHEMA  —  final_schema.sql
-- PostgreSQL 14+
-- =============================================================================
-- Canonical schema shared by:
--   • backoffice_win   (BackOffice desktop application)
--   • production_line  (Production Line desktop + TCP server)
--   • Android scanner  (mobile client via TCP server)
--
-- All three applications use core/db/db_service.cpp through the @core project.
--
-- Excluded from this schema (handled separately per project):
--   • audit_log          — to be redesigned; see *_known_issues.md
--   • event_log          — to be redesigned; see *_known_issues.md
--   • subscribers        — to be redesigned; see *_known_issues.md
--   • auth_attempts      — to be redesigned; see *_known_issues.md
--   • Event/audit triggers on items/boxes
--
-- State-machine triggers are included with an enable/disable mechanism
-- controlled by the GUC variable  app.state_triggers  ('on' / 'off').
-- =============================================================================

BEGIN;

-- =============================================================================
-- 0. CONFIGURATION  —  GUC variable for trigger control
-- =============================================================================
-- Usage:
--   SET app.state_triggers = 'off';   -- disable triggers (bulk operations)
--   SET app.state_triggers = 'on';    -- re-enable triggers (default)
--   RESET app.state_triggers;         -- reset to default ('on')
-- =============================================================================


-- =============================================================================
-- 1. PRODUCTION INFRASTRUCTURE
-- =============================================================================

CREATE TABLE IF NOT EXISTS production_lines (
    id          BIGSERIAL    PRIMARY KEY,
    name        TEXT         NOT NULL,
    description TEXT,
    active      BOOLEAN      NOT NULL DEFAULT TRUE,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT uniq_production_line_name UNIQUE (name),
    CONSTRAINT chk_production_line_name_not_empty CHECK (LENGTH(TRIM(name)) > 0)
);

CREATE INDEX IF NOT EXISTS idx_production_lines_active
    ON production_lines (active) WHERE active = TRUE;


-- =============================================================================
-- 2. PRODUCT CATALOG
-- =============================================================================

-- 2.1 Products
CREATE TABLE IF NOT EXISTS products (
    id          BIGSERIAL    PRIMARY KEY,
    gtin        TEXT,
    name        TEXT,
    description TEXT,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_products_gtin ON products (gtin);


-- 2.2 Product Packaging (box template — how many items fit in one box)
CREATE TABLE IF NOT EXISTS product_packaging (
    id                  BIGSERIAL    PRIMARY KEY,
    product_id          BIGINT,
    number_of_products  INT,
    gtin                TEXT,
    name                TEXT,
    description         TEXT,
    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT fk_product_packaging_product
        FOREIGN KEY (product_id) REFERENCES products (id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_product_packaging_gtin ON product_packaging (gtin);


-- 2.3 Package Pallet (pallet template — how many boxes fit on one pallet)
CREATE TABLE IF NOT EXISTS package_pallet (
    id                      BIGSERIAL    PRIMARY KEY,
    product_packaging_id    BIGINT,
    number_of_products      INT,
    gtin                    TEXT,
    name                    TEXT,
    description             TEXT,
    created_at              TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT fk_package_pallet_packaging
        FOREIGN KEY (product_packaging_id) REFERENCES product_packaging (id)
        ON DELETE CASCADE
);


-- =============================================================================
-- 3. CORE ENTITY TABLES
-- =============================================================================
-- Status values (shared across all apps via core::ItemStatus / BoxStatus / PalletStatus):
--   0  = Available / Empty / New
--   5  = Printed
--   10 = Read (scanned)
--   20 = Assigned / Sealed / Complete
--   30 = Exported (terminal)
--
-- Soft-delete: is_deleted + deleted_at.
-- FK production_line: ON DELETE RESTRICT — safest; prevents accidental data loss.
-- =============================================================================

-- 3.1 Items
CREATE TABLE IF NOT EXISTS items (
    id              BIGSERIAL    PRIMARY KEY,
    bar_code        TEXT         NOT NULL,
    production_line BIGINT       NOT NULL,
    status          SMALLINT     NOT NULL DEFAULT 0,
    imported_at     TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    scanned_at      TIMESTAMPTZ,
    is_deleted      BOOLEAN      NOT NULL DEFAULT FALSE,
    deleted_at      TIMESTAMPTZ,

    CONSTRAINT uniq_item_barcode UNIQUE (bar_code),
    CONSTRAINT fk_item_production_line
        FOREIGN KEY (production_line) REFERENCES production_lines (id)
        ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT chk_item_status CHECK (status IN (0, 5, 10, 20, 30)),
    CONSTRAINT chk_item_barcode_not_empty CHECK (LENGTH(TRIM(bar_code)) > 0)
);

CREATE INDEX IF NOT EXISTS idx_items_barcode           ON items (bar_code);
CREATE INDEX IF NOT EXISTS idx_items_status            ON items (status);
CREATE INDEX IF NOT EXISTS idx_items_barcode_status    ON items (bar_code, status);
CREATE INDEX IF NOT EXISTS idx_items_production_status ON items (production_line, status);
CREATE INDEX IF NOT EXISTS idx_items_status_imported   ON items (status, imported_at);
CREATE INDEX IF NOT EXISTS idx_items_scanned           ON items (scanned_at) WHERE scanned_at IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_items_available          ON items (production_line, imported_at) WHERE status = 0;
CREATE INDEX IF NOT EXISTS idx_items_not_deleted        ON items (production_line, status) WHERE NOT is_deleted;


-- 3.2 Boxes
CREATE TABLE IF NOT EXISTS boxes (
    id              BIGSERIAL    PRIMARY KEY,
    bar_code        TEXT         NOT NULL,
    production_line BIGINT       NOT NULL,
    status          SMALLINT     NOT NULL DEFAULT 0,
    imported_at     TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    sealed_at       TIMESTAMPTZ,
    is_deleted      BOOLEAN      NOT NULL DEFAULT FALSE,
    deleted_at      TIMESTAMPTZ,

    CONSTRAINT uniq_box_barcode UNIQUE (bar_code),
    CONSTRAINT fk_box_production_line
        FOREIGN KEY (production_line) REFERENCES production_lines (id)
        ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT chk_box_status CHECK (status IN (0, 5, 10, 20, 30)),
    CONSTRAINT chk_box_barcode_not_empty CHECK (LENGTH(TRIM(bar_code)) > 0)
);

CREATE INDEX IF NOT EXISTS idx_boxes_barcode           ON boxes (bar_code);
CREATE INDEX IF NOT EXISTS idx_boxes_status            ON boxes (status);
CREATE INDEX IF NOT EXISTS idx_boxes_production_status ON boxes (production_line, status);
CREATE INDEX IF NOT EXISTS idx_boxes_empty             ON boxes (production_line) WHERE status = 0;
CREATE INDEX IF NOT EXISTS idx_boxes_sealed            ON boxes (production_line, sealed_at) WHERE status = 20;
CREATE INDEX IF NOT EXISTS idx_boxes_not_deleted        ON boxes (production_line, status) WHERE NOT is_deleted;


-- 3.3 Pallets
CREATE TABLE IF NOT EXISTS pallets (
    id              BIGSERIAL    PRIMARY KEY,
    bar_code        TEXT         NOT NULL,
    production_line BIGINT       NOT NULL,
    status          SMALLINT     NOT NULL DEFAULT 0,
    created_at      TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    is_deleted      BOOLEAN      NOT NULL DEFAULT FALSE,
    deleted_at      TIMESTAMPTZ,

    CONSTRAINT uniq_pallet_barcode UNIQUE (bar_code),
    CONSTRAINT fk_pallet_production_line
        FOREIGN KEY (production_line) REFERENCES production_lines (id)
        ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT chk_pallet_status CHECK (status IN (0, 5, 10, 20, 30)),
    CONSTRAINT chk_pallet_barcode_not_empty CHECK (LENGTH(TRIM(bar_code)) > 0)
);

CREATE INDEX IF NOT EXISTS idx_pallets_barcode           ON pallets (bar_code);
CREATE INDEX IF NOT EXISTS idx_pallets_status            ON pallets (status);
CREATE INDEX IF NOT EXISTS idx_pallets_production_status ON pallets (production_line, status);
CREATE INDEX IF NOT EXISTS idx_pallets_new               ON pallets (production_line) WHERE status = 0;
CREATE INDEX IF NOT EXISTS idx_pallets_complete           ON pallets (production_line) WHERE status = 20;
CREATE INDEX IF NOT EXISTS idx_pallets_not_deleted        ON pallets (production_line, status) WHERE NOT is_deleted;


-- =============================================================================
-- 4. ASSIGNMENT JUNCTION TABLES
-- =============================================================================

-- 4.1 Item → Box   (one item belongs to at most one box)
CREATE TABLE IF NOT EXISTS item_box_assignments (
    id          BIGSERIAL    PRIMARY KEY,
    item_id     BIGINT       NOT NULL,
    box_id      BIGINT       NOT NULL,
    assigned_at TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT uniq_item_assignment UNIQUE (item_id),
    CONSTRAINT fk_assignment_item
        FOREIGN KEY (item_id) REFERENCES items (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_assignment_box
        FOREIGN KEY (box_id) REFERENCES boxes (id)
        ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_item_assignments_item         ON item_box_assignments (item_id);
CREATE INDEX IF NOT EXISTS idx_item_assignments_box          ON item_box_assignments (box_id);
CREATE INDEX IF NOT EXISTS idx_item_assignments_box_assigned ON item_box_assignments (box_id, assigned_at);


-- 4.2 Box → Pallet   (one box belongs to at most one pallet)
CREATE TABLE IF NOT EXISTS pallet_box_assignments (
    id          BIGSERIAL    PRIMARY KEY,
    box_id      BIGINT       NOT NULL,
    pallet_id   BIGINT       NOT NULL,
    assigned_at TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT uniq_box_pallet_assignment UNIQUE (box_id),
    CONSTRAINT fk_pallet_assignment_box
        FOREIGN KEY (box_id) REFERENCES boxes (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_pallet_assignment_pallet
        FOREIGN KEY (pallet_id) REFERENCES pallets (id)
        ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_pallet_assignments_box             ON pallet_box_assignments (box_id);
CREATE INDEX IF NOT EXISTS idx_pallet_assignments_pallet          ON pallet_box_assignments (pallet_id);
CREATE INDEX IF NOT EXISTS idx_pallet_assignments_pallet_assigned ON pallet_box_assignments (pallet_id, assigned_at);


-- =============================================================================
-- 5. USER MANAGEMENT
-- =============================================================================

-- 5.1 Users
CREATE TABLE IF NOT EXISTS users (
    id           BIGSERIAL    PRIMARY KEY,
    username     TEXT         NOT NULL,
    pin_hash     TEXT         NOT NULL,
    full_name    TEXT,
    email        TEXT,
    phone_number TEXT,
    active       BOOLEAN      NOT NULL DEFAULT TRUE,
    superuser    BOOLEAN      NOT NULL DEFAULT FALSE,
    created_at   TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    last_login   TIMESTAMPTZ,

    CONSTRAINT uniq_user_username UNIQUE (username),
    CONSTRAINT chk_user_username_not_empty CHECK (LENGTH(TRIM(username)) > 0)
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_users_email_unique
    ON users (email) WHERE email IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_users_username ON users (username);
CREATE INDEX IF NOT EXISTS idx_users_active   ON users (active) WHERE active = TRUE;


-- 5.2 Roles   (SERIAL — matches core::RoleId = qint32)
CREATE TABLE IF NOT EXISTS roles (
    id          SERIAL       PRIMARY KEY,
    role_name   TEXT         NOT NULL,
    description TEXT,
    active      BOOLEAN      NOT NULL DEFAULT TRUE,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT uniq_role_name UNIQUE (role_name),
    CONSTRAINT chk_role_name_not_empty CHECK (LENGTH(TRIM(role_name)) > 0)
);

CREATE INDEX IF NOT EXISTS idx_roles_active ON roles (active) WHERE active = TRUE;


-- 5.3 Permissions   (SERIAL — matches core::Permission.id = qint32)
CREATE TABLE IF NOT EXISTS permissions (
    id              SERIAL       PRIMARY KEY,
    permission_name TEXT         NOT NULL,
    category        TEXT,
    description     TEXT,
    active          BOOLEAN      NOT NULL DEFAULT TRUE,
    created_at      TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT uniq_permission_name UNIQUE (permission_name),
    CONSTRAINT chk_permission_name_not_empty CHECK (LENGTH(TRIM(permission_name)) > 0)
);

CREATE INDEX IF NOT EXISTS idx_permissions_category ON permissions (category);
CREATE INDEX IF NOT EXISTS idx_permissions_active   ON permissions (active) WHERE active = TRUE;


-- 5.4 User ↔ Role junction
CREATE TABLE IF NOT EXISTS user_roles (
    user_id     BIGINT  NOT NULL,
    role_id     INT     NOT NULL,

    PRIMARY KEY (user_id, role_id),

    CONSTRAINT fk_user_roles_user
        FOREIGN KEY (user_id) REFERENCES users (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_user_roles_role
        FOREIGN KEY (role_id) REFERENCES roles (id)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_user_roles_user ON user_roles (user_id);
CREATE INDEX IF NOT EXISTS idx_user_roles_role ON user_roles (role_id);


-- 5.5 Role ↔ Permission junction
CREATE TABLE IF NOT EXISTS role_permissions (
    role_id       INT     NOT NULL,
    permission_id INT     NOT NULL,
    granted       BOOLEAN NOT NULL DEFAULT TRUE,

    PRIMARY KEY (role_id, permission_id),

    CONSTRAINT fk_role_permissions_role
        FOREIGN KEY (role_id) REFERENCES roles (id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_role_permissions_permission
        FOREIGN KEY (permission_id) REFERENCES permissions (id)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_role_permissions_role       ON role_permissions (role_id);
CREATE INDEX IF NOT EXISTS idx_role_permissions_permission ON role_permissions (permission_id);


-- =============================================================================
-- 6. IMPORT DOCUMENT TRACKING
-- =============================================================================

-- 6.1 Items Import Documents
CREATE TABLE IF NOT EXISTS items_import_docs (
    id              BIGSERIAL    PRIMARY KEY,
    file_path       TEXT         NOT NULL,
    imported_at     TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    imported_by     BIGINT       NOT NULL,
    production_line BIGINT       NOT NULL,
    record_count    INT          NOT NULL,
    status          TEXT         NOT NULL,

    CONSTRAINT fk_items_import_docs_user
        FOREIGN KEY (imported_by) REFERENCES users (id) ON DELETE CASCADE,
    CONSTRAINT fk_items_import_docs_line
        FOREIGN KEY (production_line) REFERENCES production_lines (id) ON DELETE CASCADE
);

-- 6.2 Items Import Docs ↔ Items junction
CREATE TABLE IF NOT EXISTS items_import_docs_items (
    id             BIGSERIAL PRIMARY KEY,
    import_doc_id  BIGINT    NOT NULL,
    item_id        BIGINT    NOT NULL,

    CONSTRAINT fk_items_junction_doc
        FOREIGN KEY (import_doc_id) REFERENCES items_import_docs (id) ON DELETE CASCADE,
    CONSTRAINT fk_items_junction_item
        FOREIGN KEY (item_id) REFERENCES items (id) ON DELETE CASCADE
);

-- 6.3 Boxes Import Documents
CREATE TABLE IF NOT EXISTS boxes_import_docs (
    id              BIGSERIAL    PRIMARY KEY,
    file_path       TEXT         NOT NULL,
    imported_at     TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    imported_by     BIGINT       NOT NULL,
    production_line BIGINT       NOT NULL,
    record_count    INT          NOT NULL,
    status          TEXT         NOT NULL,

    CONSTRAINT fk_boxes_import_docs_user
        FOREIGN KEY (imported_by) REFERENCES users (id) ON DELETE CASCADE,
    CONSTRAINT fk_boxes_import_docs_line
        FOREIGN KEY (production_line) REFERENCES production_lines (id) ON DELETE CASCADE
);

-- 6.4 Boxes Import Docs ↔ Boxes junction
CREATE TABLE IF NOT EXISTS boxes_import_docs_boxes (
    id             BIGSERIAL PRIMARY KEY,
    import_doc_id  BIGINT    NOT NULL,
    box_id         BIGINT    NOT NULL,

    CONSTRAINT fk_boxes_junction_doc
        FOREIGN KEY (import_doc_id) REFERENCES boxes_import_docs (id) ON DELETE CASCADE,
    CONSTRAINT fk_boxes_junction_box
        FOREIGN KEY (box_id) REFERENCES boxes (id) ON DELETE CASCADE
);

-- 6.5 Pallets Import Documents
CREATE TABLE IF NOT EXISTS pallets_import_docs (
    id              BIGSERIAL    PRIMARY KEY,
    file_path       TEXT         NOT NULL,
    imported_at     TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    imported_by     BIGINT       NOT NULL,
    production_line BIGINT       NOT NULL,
    record_count    INT          NOT NULL,
    status          TEXT         NOT NULL,

    CONSTRAINT fk_pallets_import_docs_user
        FOREIGN KEY (imported_by) REFERENCES users (id) ON DELETE CASCADE,
    CONSTRAINT fk_pallets_import_docs_line
        FOREIGN KEY (production_line) REFERENCES production_lines (id) ON DELETE CASCADE
);

-- 6.6 Pallets Import Docs ↔ Pallets junction
CREATE TABLE IF NOT EXISTS pallets_import_docs_pallets (
    id             BIGSERIAL PRIMARY KEY,
    import_doc_id  BIGINT    NOT NULL,
    pallet_id      BIGINT    NOT NULL,

    CONSTRAINT fk_pallets_junction_doc
        FOREIGN KEY (import_doc_id) REFERENCES pallets_import_docs (id) ON DELETE CASCADE,
    CONSTRAINT fk_pallets_junction_pallet
        FOREIGN KEY (pallet_id) REFERENCES pallets (id) ON DELETE CASCADE
);


-- =============================================================================
-- 7. EXPORT TABLES
-- =============================================================================

-- 7.1 Export Documents
-- export_mode: 0 = BoxExport, 1 = PalletExport, 2 = ItemExport
CREATE TABLE IF NOT EXISTS export_documents (
    id          BIGSERIAL    PRIMARY KEY,
    export_mode SMALLINT     NOT NULL,
    lp_tin      TEXT,
    xml_content BYTEA,
    xml_hash    TEXT,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    created_by  BIGINT,

    CONSTRAINT chk_export_mode CHECK (export_mode IN (0, 1, 2)),
    CONSTRAINT fk_export_created_by
        FOREIGN KEY (created_by) REFERENCES users (id)
        ON DELETE SET NULL ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_export_documents_created ON export_documents (created_at DESC);
CREATE INDEX IF NOT EXISTS idx_export_documents_mode    ON export_documents (export_mode);
CREATE INDEX IF NOT EXISTS idx_export_documents_lp_tin  ON export_documents (lp_tin);


-- 7.2 Export Items Snapshot
CREATE TABLE IF NOT EXISTS export_items (
    id          BIGSERIAL    PRIMARY KEY,
    document_id BIGINT       NOT NULL,
    bar_code    TEXT         NOT NULL,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT fk_export_items_document
        FOREIGN KEY (document_id) REFERENCES export_documents (id)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_export_items_document ON export_items (document_id);
CREATE INDEX IF NOT EXISTS idx_export_items_barcode  ON export_items (bar_code);


-- 7.3 Export Boxes Snapshot
CREATE TABLE IF NOT EXISTS export_boxes (
    id          BIGSERIAL    PRIMARY KEY,
    document_id BIGINT       NOT NULL,
    bar_code    TEXT         NOT NULL,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT fk_export_boxes_document
        FOREIGN KEY (document_id) REFERENCES export_documents (id)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_export_boxes_document ON export_boxes (document_id);
CREATE INDEX IF NOT EXISTS idx_export_boxes_barcode  ON export_boxes (bar_code);


-- 7.4 Export Pallets Snapshot
CREATE TABLE IF NOT EXISTS export_pallets (
    id          BIGSERIAL    PRIMARY KEY,
    document_id BIGINT       NOT NULL,
    bar_code    TEXT         NOT NULL,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT fk_export_pallets_document
        FOREIGN KEY (document_id) REFERENCES export_documents (id)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_export_pallets_document ON export_pallets (document_id);
CREATE INDEX IF NOT EXISTS idx_export_pallets_barcode  ON export_pallets (bar_code);


-- =============================================================================
-- 8. STATE MACHINE TRIGGERS  (with enable/disable via GUC)
-- =============================================================================
-- Control:
--   SET app.state_triggers = 'off';   -- skip enforcement (bulk imports, migrations)
--   SET app.state_triggers = 'on';    -- enforce state machine (default)
--   RESET app.state_triggers;         -- same as 'on'
--
-- Valid forward transitions:  0 → 5 → 10 → 20 → 30
-- Reset to 0 is always allowed from any non-exported state.
-- Status 30 (Exported) is terminal.
-- =============================================================================

-- 8.1 Item State Transition
CREATE OR REPLACE FUNCTION enforce_item_state_transition()
RETURNS TRIGGER AS $$
BEGIN
    -- Check if triggers are disabled
    IF COALESCE(current_setting('app.state_triggers', true), 'on') = 'off' THEN
        RETURN NEW;
    END IF;

    IF OLD.status = 30 THEN
        RAISE EXCEPTION 'Cannot modify exported item (id=%)', OLD.id;
    END IF;

    IF OLD.status = 0 AND NEW.status NOT IN (0, 5) THEN
        RAISE EXCEPTION 'Invalid item transition: Available(0) → %. Only Printed(5) allowed.', NEW.status;
    END IF;

    IF OLD.status = 5 AND NEW.status NOT IN (5, 10, 0) THEN
        RAISE EXCEPTION 'Invalid item transition: Printed(5) → %. Only Read(10) or Available(0) allowed.', NEW.status;
    END IF;

    IF OLD.status = 10 AND NEW.status NOT IN (10, 20, 0) THEN
        RAISE EXCEPTION 'Invalid item transition: Read(10) → %. Only Assigned(20) or Available(0) allowed.', NEW.status;
    END IF;

    IF OLD.status = 20 AND NEW.status NOT IN (20, 30, 0) THEN
        RAISE EXCEPTION 'Invalid item transition: Assigned(20) → %. Only Exported(30) or Available(0) allowed.', NEW.status;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_item_state_transition ON items;
CREATE TRIGGER trg_item_state_transition
    BEFORE UPDATE OF status ON items
    FOR EACH ROW
    EXECUTE FUNCTION enforce_item_state_transition();


-- 8.2 Box State Transition
CREATE OR REPLACE FUNCTION enforce_box_state_transition()
RETURNS TRIGGER AS $$
BEGIN
    IF COALESCE(current_setting('app.state_triggers', true), 'on') = 'off' THEN
        RETURN NEW;
    END IF;

    IF OLD.status = 30 THEN
        RAISE EXCEPTION 'Cannot modify exported box (id=%)', OLD.id;
    END IF;

    IF OLD.status = 0 AND NEW.status NOT IN (0, 5) THEN
        RAISE EXCEPTION 'Invalid box transition: Empty(0) → %. Only Printed(5) allowed.', NEW.status;
    END IF;

    IF OLD.status = 5 AND NEW.status NOT IN (5, 10, 0) THEN
        RAISE EXCEPTION 'Invalid box transition: Printed(5) → %. Only Read(10) or Empty(0) allowed.', NEW.status;
    END IF;

    IF OLD.status = 10 AND NEW.status NOT IN (10, 20, 0) THEN
        RAISE EXCEPTION 'Invalid box transition: Read(10) → %. Only Sealed(20) or Empty(0) allowed.', NEW.status;
    END IF;

    IF OLD.status = 20 AND NEW.status NOT IN (20, 30, 0) THEN
        RAISE EXCEPTION 'Invalid box transition: Sealed(20) → %. Only Exported(30) or Empty(0) allowed.', NEW.status;
    END IF;

    -- Auto-manage sealed_at timestamp
    IF NEW.status = 20 AND NEW.sealed_at IS NULL THEN
        NEW.sealed_at := NOW();
    END IF;

    IF NEW.status IN (0, 5, 10) THEN
        NEW.sealed_at := NULL;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_box_state_transition ON boxes;
CREATE TRIGGER trg_box_state_transition
    BEFORE UPDATE OF status ON boxes
    FOR EACH ROW
    EXECUTE FUNCTION enforce_box_state_transition();


-- 8.3 Pallet State Transition
CREATE OR REPLACE FUNCTION enforce_pallet_state_transition()
RETURNS TRIGGER AS $$
BEGIN
    IF COALESCE(current_setting('app.state_triggers', true), 'on') = 'off' THEN
        RETURN NEW;
    END IF;

    IF OLD.status = 30 THEN
        RAISE EXCEPTION 'Cannot modify exported pallet (id=%)', OLD.id;
    END IF;

    IF OLD.status = 0 AND NEW.status NOT IN (0, 5) THEN
        RAISE EXCEPTION 'Invalid pallet transition: New(0) → %. Only Printed(5) allowed.', NEW.status;
    END IF;

    IF OLD.status = 5 AND NEW.status NOT IN (5, 10, 0) THEN
        RAISE EXCEPTION 'Invalid pallet transition: Printed(5) → %. Only Read(10) or New(0) allowed.', NEW.status;
    END IF;

    IF OLD.status = 10 AND NEW.status NOT IN (10, 20, 0) THEN
        RAISE EXCEPTION 'Invalid pallet transition: Read(10) → %. Only Complete(20) or New(0) allowed.', NEW.status;
    END IF;

    IF OLD.status = 20 AND NEW.status NOT IN (20, 30, 0) THEN
        RAISE EXCEPTION 'Invalid pallet transition: Complete(20) → %. Only Exported(30) or New(0) allowed.', NEW.status;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_pallet_state_transition ON pallets;
CREATE TRIGGER trg_pallet_state_transition
    BEFORE UPDATE OF status ON pallets
    FOR EACH ROW
    EXECUTE FUNCTION enforce_pallet_state_transition();


-- =============================================================================
-- 9. CONVENIENCE FUNCTIONS
-- =============================================================================

-- 9.1 Enable/disable state triggers for the current session
CREATE OR REPLACE FUNCTION enable_state_triggers()
RETURNS VOID LANGUAGE plpgsql AS $$
BEGIN
    PERFORM set_config('app.state_triggers', 'on', false);
END;
$$;

CREATE OR REPLACE FUNCTION disable_state_triggers()
RETURNS VOID LANGUAGE plpgsql AS $$
BEGIN
    PERFORM set_config('app.state_triggers', 'off', false);
END;
$$;


-- =============================================================================
-- 10. SEED DATA
-- =============================================================================

-- 10.1 Default Production Lines
INSERT INTO production_lines (name, description, active) VALUES
    ('Line-A', 'Primary production line',   TRUE),
    ('Line-B', 'Secondary production line', TRUE),
    ('Line-C', 'Quality control line',      TRUE)
ON CONFLICT (name) DO NOTHING;

-- 10.2 Permissions (all modules)
-- Import
INSERT INTO permissions (permission_name, description, category) VALUES
    ('import.items',    'Import items from CSV',  'import'),
    ('import.boxes',    'Import boxes from CSV',  'import'),
    ('import.pallets',  'Import pallets from CSV', 'import'),
    ('import.validate', 'Validate CSV files',     'import')
ON CONFLICT (permission_name) DO NOTHING;

-- Production
INSERT INTO permissions (permission_name, description, category) VALUES
    ('production.scan_item',     'Scan items',                    'production'),
    ('production.assign_item',   'Assign items to boxes',         'production'),
    ('production.seal_box',      'Seal boxes',                    'production'),
    ('production.assign_box',    'Assign boxes to pallets',       'production'),
    ('production.complete_pallet','Complete pallets',              'production'),
    ('production.view_stats',    'View production statistics',    'production'),
    ('production.manage_lines',  'Manage production lines',       'production')
ON CONFLICT (permission_name) DO NOTHING;

-- Export
INSERT INTO permissions (permission_name, description, category) VALUES
    ('export.create_box',      'Create box exports',      'export'),
    ('export.create_pallet',   'Create pallet exports',   'export'),
    ('export.view_documents',  'View export documents',   'export'),
    ('export.download_xml',    'Download export XML',     'export')
ON CONFLICT (permission_name) DO NOTHING;

-- Post-Production
INSERT INTO permissions (permission_name, description, category) VALUES
    ('postprod.mark_damaged',      'Mark items as damaged',       'postprod'),
    ('postprod.remove_item',       'Remove items from boxes',     'postprod'),
    ('postprod.replace_item',      'Replace items in boxes',      'postprod'),
    ('postprod.unseal_box',        'Unseal boxes',                'postprod'),
    ('postprod.remove_box',        'Remove boxes from pallets',   'postprod'),
    ('postprod.replace_box',       'Replace boxes on pallets',    'postprod'),
    ('postprod.uncomplete_pallet', 'Uncomplete pallets',          'postprod'),
    ('postprod.view_audit',        'View audit logs',             'postprod')
ON CONFLICT (permission_name) DO NOTHING;

-- User Management
INSERT INTO permissions (permission_name, description, category) VALUES
    ('user.create',             'Create users',              'user'),
    ('user.update',             'Update users',              'user'),
    ('user.delete',             'Delete users',              'user'),
    ('user.view',               'View users',                'user'),
    ('user.manage_roles',       'Manage user roles',         'user'),
    ('role.create',             'Create roles',              'role'),
    ('role.update',             'Update roles',              'role'),
    ('role.delete',             'Delete roles',              'role'),
    ('role.manage_permissions', 'Manage role permissions',   'role')
ON CONFLICT (permission_name) DO NOTHING;

-- 10.3 Default Roles
INSERT INTO roles (role_name, description, active) VALUES
    ('Administrator', 'Full system access',                     TRUE),
    ('Operator',      'Production operations',                  TRUE),
    ('Viewer',        'Read-only access',                       TRUE),
    ('Supervisor',    'Production and post-production access',  TRUE)
ON CONFLICT (role_name) DO NOTHING;

-- 10.4 Assign Permissions to Roles

-- Administrator gets all permissions
INSERT INTO role_permissions (role_id, permission_id, granted)
SELECT r.id, p.id, TRUE
FROM roles r, permissions p
WHERE r.role_name = 'Administrator'
ON CONFLICT (role_id, permission_id) DO NOTHING;

-- Operator gets production + import permissions
INSERT INTO role_permissions (role_id, permission_id, granted)
SELECT r.id, p.id, TRUE
FROM roles r, permissions p
WHERE r.role_name = 'Operator'
  AND p.category IN ('production', 'import')
ON CONFLICT (role_id, permission_id) DO NOTHING;

-- Viewer gets view permissions only
INSERT INTO role_permissions (role_id, permission_id, granted)
SELECT r.id, p.id, TRUE
FROM roles r, permissions p
WHERE r.role_name = 'Viewer'
  AND p.permission_name IN ('production.view_stats', 'export.view_documents',
                            'postprod.view_audit', 'user.view')
ON CONFLICT (role_id, permission_id) DO NOTHING;

-- Supervisor gets production + post-production + export + import
INSERT INTO role_permissions (role_id, permission_id, granted)
SELECT r.id, p.id, TRUE
FROM roles r, permissions p
WHERE r.role_name = 'Supervisor'
  AND p.category IN ('production', 'postprod', 'export', 'import')
ON CONFLICT (role_id, permission_id) DO NOTHING;

-- 10.5 Default Admin User  (PIN = '0000', SHA-256)
INSERT INTO users (username, pin_hash, full_name, email, active, superuser) VALUES
    ('admin',
     '9af15b336e6a9619928537df30b2e6a2376569fcf9d7e773eccede65606529a0',
     'System Administrator', 'admin@system.local', TRUE, TRUE)
ON CONFLICT (username) DO NOTHING;

-- Assign Administrator role to admin user
INSERT INTO user_roles (user_id, role_id)
SELECT u.id, r.id
FROM users u, roles r
WHERE u.username = 'admin' AND r.role_name = 'Administrator'
ON CONFLICT (user_id, role_id) DO NOTHING;


COMMIT;

-- =============================================================================
-- SCHEMA SUMMARY
-- =============================================================================
-- Tables (24):
--   1.  production_lines           6.  boxes                  11. permissions
--   2.  products                   7.  pallets                12. user_roles
--   3.  product_packaging          8.  item_box_assignments   13. role_permissions
--   4.  package_pallet             9.  pallet_box_assignments 14-19. import docs + junctions (x6)
--   5.  items                     10.  users                  20-24. export docs + snapshots (x5)
--                                     roles
--
-- Triggers (3):
--   trg_item_state_transition   — enforces item status state machine
--   trg_box_state_transition    — enforces box status state machine
--   trg_pallet_state_transition — enforces pallet status state machine
--
-- Functions (5):
--   enforce_item_state_transition()
--   enforce_box_state_transition()
--   enforce_pallet_state_transition()
--   enable_state_triggers()
--   disable_state_triggers()
--
-- Excluded (handled per-project):
--   audit_log, event_log, subscribers, auth_attempts
-- =============================================================================
